#define STB_IMAGE_IMPLEMENTATION
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <fstream>
#include <strstream>
#include "miniVM.h"
#include "OBJLoader.h"
#include "stb_image.h"
#include "geometry.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "particleSimulation.h"
#include "shader.h"
#include "ParticleRenderer.h"
#include "ply_loader.h"
#include "3DGS_renderer.h"
#include "sort.h"
using namespace std; 

const int Height = 1200;
const int Width = 1200;
const float FOV = 90.0f;
const float PI = 3.1415;
const float Zfar = 1000.0f;
const float Znear = 0.1;


float fTheta = 0.0f;
float fYaw = 0;
vec3f vCamera = {0,0,0};
vec3f up = {0,1,0};
vec3f lookDir = {0,0,1};
vec3f LookatTarget = {0,0,1};
mat4x4 CameraMatrix;
mat4x4 view;
mat4x4 model;
mat4x4 rotXMat;
mat4x4 transMat;
mat4x4 projection;
MeshGPU gMesh;
GLuint gTex0 = 0;

ParticleRenderer particleRenderer;


bool showDemoWindow = false;
bool showControlWindow = true;
float lightDirection[3] = {0.0f, 0.0f, 1.0f};
float modelPosition[3] = {0.0f, 0.0f, 1.0f};
float modelRotation[3] = {0.0f, 0.0f, 0.0f};
float backgroundColor[3] = {0.2f, 0.5f, 0.5f};
float cameraPosition[3] = {0.0f, 0.0f, 0.0f};
float lightColor[3] = {1.0f , 1.0f, 1.0f};
float specularStrength = 0.8f;
float ambientStrength = 0.2f;
float cameraYaw = 0.0f;
GLuint LoadTexture2D(const std::string& path)
{
    int width, height, channels;
    //stbi_set_flip_vertically_on_load(true); 
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "STB failed to load: " << path << " - Error: " << stbi_failure_reason() << "\n";
        return 0;
    }
    
    GLenum format;
    if (channels == 1)
        format = GL_RED;
    else if (channels == 3)
        format = GL_RGB;
    else if (channels == 4)
        format = GL_RGBA;
    else {
        stbi_image_free(data);
        return 0;
    }
    
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    stbi_image_free(data);
    return tex;
}
unsigned int compileShader(GLenum type, const char* source)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    // Check for compilation errors
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::" << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") 
             << "::COMPILATION_FAILED\n" << infoLog << endl;
    }
    return shader;
}
shader setupOpenGL()
{
    
    shader graphicsShader("C:\\Dev\\git\\PROJECT_NGINE\\PROJECT_NGINE\\src\\shaders\\shader.vs", "C:\\Dev\\git\\PROJECT_NGINE\\PROJECT_NGINE\\src\\shaders\\shader.fs");
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    return graphicsShader;
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
void initialize(vector<Gaussian>& gaussians, AppArgs& args)
{
    
    std::vector<Vertex> verts; 
    std::vector<uint32_t> indices;
    bool ok = LoadOBJ_Indexed("C:\\Dev\\git\\PROJECT_NGINE\\PROJECT_NGINE\\POT.obj", verts, indices);
    std::cout << "OBJ ok=" << ok
          << " verts=" << verts.size()
          << " indices=" << indices.size()
          << "\n";
    const float aspect = (float)Width/(float)Height;
    projection = matrix_makeProjection(FOV, aspect, Znear, Zfar);

    gMesh.upload(verts, indices);
    gTex0 = LoadTexture2D("C:\\Dev\\git\\PROJECT_NGINE\\PROJECT_NGINE\\src\\diffuse.png");
    if(!gTex0)
    {
        cout << "failed to load texture\n";
        exit(1);
    }

    // load ply for 3dgs if using 3dgs
    gaussians = load_ply(args.ply_path);
    if (gaussians.empty()) {
        std::cerr << "[Main] No Gaussians loaded, exiting.\n";
        return 1;
    }
    
}
void setupImGui(GLFWwindow* window)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");
}
void renderImGui()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Update UI variables from game state
    cameraPosition[0] = vCamera.x;
    cameraPosition[1] = vCamera.y;
    cameraPosition[2] = vCamera.z;
    cameraYaw = fYaw;

    // 2. Show control window
    if (showControlWindow)
    {
        ImGui::Begin("Controls", &showControlWindow);
        
        // Simulation Controls - NEW SECTION
        if (ImGui::CollapsingHeader("SPH Simulation Parameters"))
        {
            // Static parameters
            ImGui::Text("Simulation Settings");
            ImGui::Separator();
            
            ImGui::Text("Particle Count: %d", PARTICLE_COUNT);
            ImGui::Text("Box Min: (%.2f, %.2f, %.2f)", 
                       BOX_MIN.x, BOX_MIN.y, BOX_MIN.z);
            ImGui::Text("Box Max: (%.2f, %.2f, %.2f)", 
                       BOX_MAX.x, BOX_MAX.y, BOX_MAX.z);
            ImGui::Text("PI Value: %.2f", M_PI);
            
            // Editable parameters (if they're modifiable in simulator)
            static float smoothingRadius = SMOOTHING_RADIUS;
            static float particleMass = PARTICLE_MASS;
            static float restDensity = REST_DENSITY;
            static float pressureConstant = PRESSURE_CONSTANT;
            static float viscosityConstant = VISCOSITY_CONSTANT;
            static float gravity = GRAVITY;
            static float bounceDamping = BOUNCE_DAMPING;
            static float timeStep = TIME_STEP;
            
            if (ImGui::DragFloat("Smoothing Radius", &smoothingRadius, 0.001f, 0.01f, 100.0f, "%.4f"))
            {
                // Update simulator if parameter is modifiable
                // simulator.setSmoothingRadius(smoothingRadius);
                SMOOTHING_RADIUS = smoothingRadius;
            }
            
            if (ImGui::DragFloat("Particle Mass", &particleMass, 0.01f, 0.001f, 100.0f, "%.4f"))
            {
                // simulator.setParticleMass(particleMass);
                PARTICLE_MASS = particleMass;
            }
            
            if (ImGui::DragFloat("Rest Density", &restDensity, 10.0f, 100.0f, 5000.0f, "%.1f"))
            {
                // simulator.setRestDensity(restDensity);
                REST_DENSITY = restDensity;
            }
            
            if (ImGui::DragFloat("Pressure Constant", &pressureConstant, 1.0f, 0.0f, 1000.0f, "%.1f"))
            {
                // simulator.setPressureConstant(pressureConstant);
                PRESSURE_CONSTANT = pressureConstant;
            }
            
            if (ImGui::DragFloat("Viscosity Constant", &viscosityConstant, 0.001f, 0.0f, 1.0f, "%.4f"))
            {
                // simulator.setViscosityConstant(viscosityConstant);
                VISCOSITY_CONSTANT = viscosityConstant;
            }
            
            if (ImGui::DragFloat("Gravity", &gravity, 0.1f, -20.0f, 0.0f, "%.2f"))
            {
                // simulator.setGravity(gravity);
                GRAVITY = gravity;
            }
            
            if (ImGui::DragFloat("Bounce Damping", &bounceDamping, 0.01f, 0.0f, 1.0f, "%.2f"))
            {
                // simulator.setBounceDamping(bounceDamping);
                BOUNCE_DAMPING = bounceDamping;
            }
            
            if (ImGui::DragFloat("Time Step", &timeStep, 0.0001f, 0.0001f, 0.1f, "%.4f"))
            {
                // simulator.setTimeStep(timeStep);
                TIME_STEP = timeStep;
            }
            
            // Reset button
            if (ImGui::Button("Reset to Defaults"))
            {
                smoothingRadius = SMOOTHING_RADIUS;
                particleMass = PARTICLE_MASS;
                restDensity = REST_DENSITY;
                pressureConstant = PRESSURE_CONSTANT;
                viscosityConstant = VISCOSITY_CONSTANT;
                gravity = GRAVITY;
                bounceDamping = BOUNCE_DAMPING;
                timeStep = TIME_STEP;
                
                // Reset simulator parameters
                // simulator.resetParameters();
            }
            
            // Simulation controls
            ImGui::Separator();
            ImGui::Text("Simulation Controls");
            
            static bool isPaused = false;
            if (ImGui::Button(isPaused ? "Resume" : "Pause"))
            {
                isPaused = !isPaused;
                // simulator.setPaused(isPaused);
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Reset Simulation"))
            {
                // simulator.reset();
            }
            
            // Performance info
            ImGui::Separator();
            ImGui::Text("Performance");
            ImGui::Text("Particles: %d", PARTICLE_COUNT);
            // Add more performance stats if available
        }

        // Model Controls
        if (ImGui::CollapsingHeader("Model Controls"))
        {
            ImGui::Text("Model Position");
            if (ImGui::DragFloat3("Position", modelPosition, 0.01f, -10.0f, 10.0f))
            {
                // Update model position
                transMat = matrix_makeTranslation(modelPosition[0], modelPosition[1], modelPosition[2]);
            }
            
            ImGui::Text("Model Rotation");
            ImGui::DragFloat3("Rotation (degrees)", modelRotation, 1.0f, -180.0f, 180.0f);
        }
        
        // Camera Controls
        if (ImGui::CollapsingHeader("Camera Controls"))
        {
            ImGui::Text("Camera Position");
            ImGui::DragFloat3("Camera Pos", cameraPosition, 0.01f, -10.0f, 10.0f);
            
            ImGui::Text("Camera Rotation");
            ImGui::DragFloat("Yaw (degrees)", &cameraYaw, 1.0f, -180.0f, 180.0f);
            
            if (ImGui::Button("Reset Camera"))
            {
                vCamera = {0, 0, 0};
                fYaw = 0.0f;
            }
        }
        
        // Lighting Controls
        if (ImGui::CollapsingHeader("Lighting"))
        {   
            ImGui::ColorEdit3("Light Color", lightColor);
            ImGui::Text("Light Direction");
            ImGui::DragFloat3("Direction", lightDirection, 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat("specular strength", &specularStrength, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("ambient strength", &ambientStrength, 0.01f, 0.0f, 1.0f);
            
            ImGui::Text("Visualization");
            ImGui::ColorEdit3("Background Color", backgroundColor);
        }
        
        // Render Stats
        if (ImGui::CollapsingHeader("Statistics"))
        {
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 
                        1000.0f / ImGui::GetIO().Framerate, 
                        ImGui::GetIO().Framerate);
            ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", 
                        vCamera.x, vCamera.y, vCamera.z);
            ImGui::Text("Camera Yaw: %.2f degrees", fYaw * 180.0f / PI);
        }
        
        // Help
        if (ImGui::CollapsingHeader("Help"))
        {
            ImGui::Text("Camera Controls:");
            ImGui::BulletText("W/S: Move camera up/down");
            ImGui::BulletText("A/D: Move camera left/right");
            ImGui::BulletText("Up/Down Arrow: Move forward/back");
            ImGui::BulletText("Left/Right Arrow: Rotate camera");
            ImGui::Separator();
            ImGui::Text("UI Controls:");
            ImGui::BulletText("Click and drag sliders to adjust values");
            ImGui::BulletText("Check 'Show Demo Window' for more ImGui examples");
        }
        
        ImGui::Separator();
        ImGui::Checkbox("Show Demo Window", &showDemoWindow);
        
        ImGui::End();
    }

    // Rendering
    ImGui::Render();
}
void checkGLError(const char* context) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error in " << context << ": " << error << std::endl;
    }
}
void renderloop(GLFWwindow* window, shader gfxShader, SPHSimulator& simulator, GaussianRenderer gaussianRenderer, vector<int>& gaussians)
{   
    // input (only if ImGui isn't capturing mouse/keyboard)
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard)
    {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            vCamera.y += 0.03f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            vCamera.y -= 0.03f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            vCamera.x += 0.03f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            vCamera.x -= 0.03f;

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            fYaw -= 2.0f * (PI / 180.0f);
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            fYaw += 2.0f * (PI / 180.0f);
        vec3f camForwardV = vector_mul(lookDir, 0.01f);
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            vCamera = vector_sub(vCamera, camForwardV);
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            vCamera = vector_add(vCamera, camForwardV);
    }

    glClearColor(backgroundColor[0], backgroundColor[1], backgroundColor[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    simulator.step();
    gfxShader.use();
    fTheta += 0.1;
    rotXMat = matrix_makeRotationY(0);
    transMat = matrix_makeTranslation(modelPosition[0], modelPosition[1], modelPosition[2]); 
    
    model = matrix_makeIdentity();
    model = transMat;
    
    mat4x4 matCamRot = matrix_makeRotationY(fYaw);
    vec3f LookatTarget = {0,0,1};
    lookDir = vectorMatMul(LookatTarget, matCamRot);
    lookDir = vector_normalize(lookDir);
    LookatTarget = vector_add(vCamera, lookDir);

    CameraMatrix = matrix_pointAt(vCamera, LookatTarget, up);
    view = matrix_quickInvert(CameraMatrix);
    
    auto setMat = [&](const char* name, const mat4x4& M)
    {
        float m[16] = 
        {
            M.m[0][0], M.m[0][1], M.m[0][2], M.m[0][3],
            M.m[1][0], M.m[1][1], M.m[1][2], M.m[1][3],
            M.m[2][0], M.m[2][1], M.m[2][2], M.m[2][3],
            M.m[3][0], M.m[3][1], M.m[3][2], M.m[3][3]
        };

        //glUniformMatrix4fv(glGetUniformLocation(shaderProgram, name), 1, GL_FALSE, m);
        gfxShader.setMat4(name, m);
    };
    setMat("model", model);
    setMat("view", view);
    setMat("projection", projection);


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTex0);

    gfxShader.setFloat3("uLightDirection",lightDirection[0], lightDirection[1], lightDirection[2]);
    gfxShader.setFloat3("lightColor",lightColor[0], lightColor[1], lightColor[2]);
    gfxShader.setFloat3("viewPos", cameraPosition[0], cameraPosition[1], cameraPosition[2]);
    gfxShader.setFloat("specularStrength", specularStrength);
    gfxShader.setFloat("ambientStrength", ambientStrength);
    gfxShader.setInt("uTex0", 0);
    
    
    gMesh.draw();

    vector<int> sorted_indices = compute_sorted_indices(gaussians, view);
    gaussianRenderer.render(gaussians, sorted_indices, cam);
    glDepthFunc(GL_LEQUAL); // Allow particles to blend with existing geometry

    // DEBUG: Check if particle buffer is valid
    GLuint particleBuffer = simulator.getParticleBuffer();
    if (particleBuffer == 0) {
        std::cout << "ERROR: Particle buffer is 0!" << std::endl;
    } else {
        GLint bufferSize;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBuffer);
        glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &bufferSize);
        //std::cout << "Particle buffer size: " << bufferSize << " bytes, expected: " 
        //          << (PARTICLE_COUNT * sizeof(Particle)) << " bytes" << std::endl;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }


    particleRenderer.render(simulator.getParticleBuffer(), PARTICLE_COUNT, view, projection);
    checkGLError("particleRenderer.render");
    glDepthFunc(GL_LESS); // Restore default depth function
    
    // Render ImGui on top
    renderImGui();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void cleanupImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
int main(int argc, char** argv)
{   
    
    AppArgs args = parse_args(argc, argv);
    if (!glfwInit())
    {
        cout << "Failed to initialize GLFW" << endl;
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window;
    window = glfwCreateWindow(Width, Height, "NGine", NULL, NULL);
    if (window == NULL)
    {
        cout << "Failed to open GLFW window" << endl;
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    glViewport(0, 0, Width, Height);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    
    
    setupImGui(window);
    shader gfxShader = setupOpenGL();
    initialize();
    
    // simulation
    SPHSimulator simulator;
    particleRenderer.init();

    GaussianRenderer gaussianRenderer;
    gaussianRenderer.init();
    // Enable vsync
    glfwSwapInterval(1);

    while(!glfwWindowShouldClose(window))
    {
        renderloop(window, gfxShader, simulator);
        glfwSwapBuffers(window);
        glfwPollEvents();    
    }

    // Cleanup
    cleanupImGui();
    gMesh.destroy();
    if (gTex0) glDeleteTextures(1, &gTex0);
    gfxShader.deleteProgram();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}