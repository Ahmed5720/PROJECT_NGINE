#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"

#include <iostream>
#include <vector>
#include <random>
#include <cmath>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"


static const int   kNumParticles = 5000;
static const int   kLocalSize = 64;
static const float PI = 3.14159265358979323846f;



static inline GLuint CeilDiv(GLuint n, GLuint d) { return (n + d - 1) / d; }

// Match compute shader layouts
struct ParticleCPU {
    glm::vec4 posLife; // xyz pos, w life
    glm::vec4 color;
    glm::vec4 velPad;  // xyz vel
};

struct SPHDataCPU {
    glm::vec4 densPres;
    glm::vec4 forcePad;
};

// std140-friendly: 6 vec4 = 96 bytes
struct SimParamsUBO {
    glm::vec4 p0;     // (dt, bounce, count, gravity)
    glm::vec4 p1;     // (h, h2, mass, rest_density)
    glm::vec4 p2;     // (pressure_k, viscosity_k, 0, 0)
    glm::vec4 boxMin; // xyz, 0
    glm::vec4 boxMax; // xyz, 0
    glm::vec4 kernel; // (poly6_const, spiky_const, 0, 0)
};

// std140: mat4 (64) + vec4 right + vec4 up = 96 bytes
struct CameraUBO {
    glm::mat4 mvp;
    glm::vec4 right;
    glm::vec4 up;
};

static void glfwErrorCb(int code, const char* msg) {
    std::cerr << "GLFW error " << code << ": " << msg << "\n";
}

struct SimUI {
    bool  simulate = true;
    float dt = 0.01f;
    float bounce = 0.5f;
    float gravity = -9.8f;

    float smoothing_radius = 0.1f;     // must start larger than 0.01 
    float mass = 1.0f;
    float rest_density = 625.0f;
    float pressure_k = 1.0f;
    float viscosity_k = 0.0f;

    glm::vec3 boxMin = glm::vec3(-1.f, -1.f, -1.f);
    glm::vec3 boxMax = glm::vec3(1.f, 1.f, 1.f);

    // derived (display)
    float smoothing_radius2 = 0.0f;
    float poly6_const = 0.0f;
    float spiky_const = 0.0f;
};


//recompute every time sim parameters are updated
static inline void RecomputeKernel(SimUI& s) {
    s.smoothing_radius2 = s.smoothing_radius * s.smoothing_radius;

    float h = s.smoothing_radius;
    float h2 = s.smoothing_radius2;
    float h6 = h2 * h2 * h2;
    float h9 = h6 * h2 * h;

    // same as your WGSL
    s.poly6_const = 315.0f / (64.0f * PI * h9);
    s.spiky_const = -45.0f / (PI * h6);
}

static inline void UpdateSimUBO(GLuint simUBO, const SimUI& ui, int particleCount) {
    SimParamsUBO sim{};
    sim.p0 = glm::vec4(ui.simulate ? ui.dt : 0.0f, ui.bounce, (float)particleCount, ui.gravity);
    sim.p1 = glm::vec4(ui.smoothing_radius, ui.smoothing_radius2, ui.mass, ui.rest_density);
    sim.p2 = glm::vec4(ui.pressure_k, ui.viscosity_k, 0.0f, 0.0f);
    sim.boxMin = glm::vec4(ui.boxMin, 0.0f);
    sim.boxMax = glm::vec4(ui.boxMax, 0.0f);
    sim.kernel = glm::vec4(ui.poly6_const, ui.spiky_const, 0.0f, 0.0f);

    glBindBuffer(GL_UNIFORM_BUFFER, simUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParamsUBO), &sim);
}

int main() {
    glfwSetErrorCallback(glfwErrorCb);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(1280, 720, "SPH simulation", nullptr, nullptr);
    if (!win) return 1;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return 1;
    }

    // ---- ImGui init ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 430");



    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    Shader render("src/shaders/render.vert", "src/shaders/render.frag");
    Shader densityCS("src/shaders/density.comp");
    Shader forcesCS("src/shaders/forces.comp");
    Shader integrateCS("src/shaders/integrate.comp");

  

    // ----- Create buffers -----
    GLuint particlesBuf = 0, sphBuf = 0, simUBO = 0, camUBO = 0;
    glGenBuffers(1, &particlesBuf);
    glBindBuffer(GL_ARRAY_BUFFER, particlesBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ParticleCPU) * kNumParticles, nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &sphBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphBuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(SPHDataCPU) * kNumParticles, nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &simUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, simUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SimParamsUBO), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &camUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, camUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBO), nullptr, GL_DYNAMIC_DRAW);

    // Bind SSBO/UBO base bindings to match GLSL
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particlesBuf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sphBuf);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, camUBO);

    // ----- Init data on CPU then upload -----
    std::vector<ParticleCPU> particles(kNumParticles);
    std::vector<SPHDataCPU>  sph(kNumParticles);

    std::mt19937 rng(1234);
    auto randf = [&](float a, float b) {
        std::uniform_real_distribution<float> dist(a, b);
        return dist(rng);
        };

    glm::vec3 boxMin(-1.f, -1.f, -1.f);
    glm::vec3 boxMax(1.f, 1.f, 1.f);

    for (int i = 0; i < kNumParticles; ++i) {
        glm::vec3 p(randf(boxMin.x, boxMax.x), randf(boxMin.y, boxMax.y), randf(boxMin.z, boxMax.z));
        glm::vec3 v(randf(-0.2f, 0.2f), randf(-0.2f, 0.2f), randf(-0.2f, 0.2f));

        particles[i].posLife = glm::vec4(p, 1.0f);
        particles[i].color = glm::vec4(0.2f, 0.1f, 0.9f, 1.0f);
        particles[i].velPad = glm::vec4(v, 0.0f);

        sph[i].densPres = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        sph[i].forcePad = glm::vec4(0.0f);
    }

    glBindBuffer(GL_ARRAY_BUFFER, particlesBuf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ParticleCPU) * kNumParticles, particles.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphBuf);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(SPHDataCPU) * kNumParticles, sph.data());

    // ----- Quad vertex buffer (two triangles, 6 verts) -----
    float quadVerts[12] = {
        -1.f,-1.f,  +1.f,-1.f,  -1.f,+1.f,
        -1.f,+1.f,  +1.f,-1.f,  +1.f,+1.f
    };

    GLuint quadVBO = 0;
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    // ----- VAO: instance attrs from particlesBuf + per-vertex from quadVBO -----
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Instance buffer as vertex input (same buffer object as SSBO)
    glBindBuffer(GL_ARRAY_BUFFER, particlesBuf);

    // location 0: instPos (vec3) at offset 0, stride 48
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleCPU), (void*)0);
    glVertexAttribDivisor(0, 1);

    // location 1: instColor (vec4) at offset 16
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleCPU), (void*)(sizeof(glm::vec4)));
    glVertexAttribDivisor(1, 1);

    // Quad verts at location 2
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
    glVertexAttribDivisor(2, 0);

    glBindVertexArray(0);

    // ----- Simulation params -----
    float h = 0.01f;
    float h2 = h * h;
    float h6 = h2 * h2 * h2;
    float h9 = h6 * h2 * h;

    float poly6_const = 315.0f / (64.0f * PI * h9);
    float spiky_const = -45.0f / (PI * h6);

    SimParamsUBO sim{};
    sim.p0 = glm::vec4(0.01f, 0.5f, (float)kNumParticles, -9.8f);
    sim.p1 = glm::vec4(h, h2, 1.0f, 625.0f);
    sim.p2 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    sim.boxMin = glm::vec4(boxMin, 0.0f);
    sim.boxMax = glm::vec4(boxMax, 0.0f);
    sim.kernel = glm::vec4(poly6_const, spiky_const, 0.0f, 0.0f);

    // Camera
    CameraUBO cam{};
    // set per-frame below

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        int w, hpx;
        glfwGetFramebufferSize(win, &w, &hpx);
        glViewport(0, 0, w, hpx);




        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static SimUI ui;
        static bool uiInit = false;
        if (!uiInit) {
            RecomputeKernel(ui);
            uiInit = true;
        }

        bool kernelChanged = false;

        ImGui::Begin("SPH Controls");

        ImGui::Text("Particles: %d", kNumParticles);
        ImGui::Checkbox("Simulate", &ui.simulate);

        ImGui::SliderFloat("dt", &ui.dt, 0.001f, 0.05f, "%.4f");
        ImGui::SliderFloat("Gravity", &ui.gravity, -30.0f, 30.0f, "%.2f");
        ImGui::SliderFloat("Bounce", &ui.bounce, 0.0f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::Text("SPH");

        kernelChanged |= ImGui::SliderFloat("Smoothing radius (h)", &ui.smoothing_radius, 0.01f, 0.5f, "%.3f");
        ImGui::SliderFloat("Mass", &ui.mass, 0.001f, 10.0f, "%.3f");
        ImGui::SliderFloat("Rest density", &ui.rest_density, 0.1f, 2000.0f, "%.1f");
        ImGui::SliderFloat("Pressure k", &ui.pressure_k, 0.0f, 5000.0f, "%.1f");
        ImGui::SliderFloat("Viscosity k", &ui.viscosity_k, 0.0f, 5.0f, "%.4f");

        ImGui::Separator();
        ImGui::Text("Bounds");
        ImGui::DragFloat3("Box Min", &ui.boxMin.x, 0.01f);
        ImGui::DragFloat3("Box Max", &ui.boxMax.x, 0.01f);

        if (ui.boxMax.x <= ui.boxMin.x) ui.boxMax.x = ui.boxMin.x + 0.01f;
        if (ui.boxMax.y <= ui.boxMin.y) ui.boxMax.y = ui.boxMin.y + 0.01f;
        if (ui.boxMax.z <= ui.boxMin.z) ui.boxMax.z = ui.boxMin.z + 0.01f;

        ImGui::Separator();
        ImGui::Text("Derived (read-only)");
        ImGui::Text("h^2: %.6f", ui.smoothing_radius2);
        ImGui::Text("poly6: %.6e", ui.poly6_const);
        ImGui::Text("spiky: %.6e", ui.spiky_const);

        ImGui::End();

        if (kernelChanged) {
            RecomputeKernel(ui);
        }

        // Push parameters into UBO *every frame*
        UpdateSimUBO(simUBO, ui, kNumParticles);



        // Update simulation UBO
        glBindBuffer(GL_UNIFORM_BUFFER, simUBO);
        //glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParamsUBO), &sim);

        // Compute camera matrices
        float aspect = (hpx == 0) ? 1.0f : (float)w / (float)hpx;
        glm::mat4 proj = glm::perspective(2.0f * PI / 5.0f, aspect, 1.0f, 100.0f);
        glm::mat4 view(1.0f);
        view = glm::translate(view, glm::vec3(0, 0, -3));
        view = glm::rotate(view, -0.2f * PI, glm::vec3(1, 45, 0));

        cam.mvp = proj * view;
        cam.right = glm::vec4(glm::vec3(view[0]), 0.0f); // first column
        cam.up = glm::vec4(glm::vec3(view[1]), 0.0f); // second column

        glBindBuffer(GL_UNIFORM_BUFFER, camUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBO), &cam);

        // ----- Compute passes -----
        GLuint groups = CeilDiv((GLuint)kNumParticles, (GLuint)kLocalSize);

        densityCS.use();
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        forcesCS.use();
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        integrateCS.use();
        glDispatchCompute(groups, 1, 1);

        // Makes sure vertex stage sees updated particle buffer
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        // ----- Render -----
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render.use();
        glBindVertexArray(vao);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, kNumParticles);
        glBindVertexArray(0);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        //glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glfwSwapBuffers(win);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &quadVBO);
    glDeleteBuffers(1, &particlesBuf);
    glDeleteBuffers(1, &sphBuf);
    glDeleteBuffers(1, &simUBO);
    glDeleteBuffers(1, &camUBO);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}

