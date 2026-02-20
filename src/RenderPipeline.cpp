#include "RenderPipeline.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <iostream>

namespace {
const float PI = 3.14159265f;

void setMat4(shader& s, const char* name, const mat4x4& M) {
    float m[16] = {
        M.m[0][0], M.m[0][1], M.m[0][2], M.m[0][3],
        M.m[1][0], M.m[1][1], M.m[1][2], M.m[1][3],
        M.m[2][0], M.m[2][1], M.m[2][2], M.m[2][3],
        M.m[3][0], M.m[3][1], M.m[3][2], M.m[3][3]
    };
    s.setMat4(name, m);
}

void checkGLError(const char* context) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        std::cerr << "OpenGL error in " << context << ": " << err << std::endl;
}
}  // namespace

RenderPipeline::RenderPipeline(shader* phongShader,
                               GaussianRenderer* gaussianRenderer,
                               ParticleRenderer* particleRenderer)
    : phongShader_(phongShader),
      gaussianRenderer_(gaussianRenderer),
      particleRenderer_(particleRenderer) {}

void RenderPipeline::render(Scene& scene,
                            SPHSimulator& simulator,
                            int viewportW,
                            int viewportH,
                            float zNear,
                            float zFar) {
    glClearColor(scene.backgroundColor[0], scene.backgroundColor[1], scene.backgroundColor[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)viewportW / (float)viewportH;
    mat4x4 view = scene.camera.getViewMatrix();
    mat4x4 projection = scene.camera.getProjectionMatrix(aspect, zNear, zFar);

    mat4x4 model = matrix_makeTranslation(
        scene.modelPosition[0], scene.modelPosition[1], scene.modelPosition[2]);

    phongShader_->use();
    setMat4(*phongShader_, "model", model);
    setMat4(*phongShader_, "view", view);
    setMat4(*phongShader_, "projection", projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene.textureId);
    phongShader_->setFloat3("uLightDirection",
        scene.lightDirection[0], scene.lightDirection[1], scene.lightDirection[2]);
    phongShader_->setFloat3("lightColor",
        scene.lightColor[0], scene.lightColor[1], scene.lightColor[2]);
    phongShader_->setFloat3("viewPos",
        scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);
    phongShader_->setFloat("specularStrength", scene.specularStrength);
    phongShader_->setFloat("ambientStrength", scene.ambientStrength);
    phongShader_->setInt("uTex0", 0);

    scene.mesh.draw();

    if (!scene.gaussians.empty()) {
        std::vector<int> sortedIndices = compute_sorted_indices(scene.gaussians, view);

        // Gaussian splats: blend back-to-front (order from sort). No depth test so
        // nothing is culled by the mesh depth buffer.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);

        gaussianRenderer_->render(scene.gaussians, sortedIndices, view, projection,
                                  viewportW, viewportH, scene.camera.fovDeg);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

    }

    glDepthFunc(GL_LEQUAL);
    GLuint particleBuffer = simulator.getParticleBuffer();
    if (particleBuffer != 0) {
        particleRenderer_->render(particleBuffer, PARTICLE_COUNT, view, projection);
        checkGLError("particleRenderer.render");
    }
    glDepthFunc(GL_LESS);

    renderImGui(scene, PI);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RenderPipeline::renderImGui(Scene& scene, float pi) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    float cameraYawDeg = scene.camera.yaw * (180.0f / pi);

    if (scene.showControlWindow) {
        ImGui::Begin("Controls", &scene.showControlWindow);

        if (ImGui::CollapsingHeader("SPH Simulation Parameters")) {
            ImGui::Text("Simulation Settings");
            ImGui::Separator();
            ImGui::Text("Particle Count: %d", PARTICLE_COUNT);
            ImGui::Text("Box Min: (%.2f, %.2f, %.2f)", BOX_MIN.x, BOX_MIN.y, BOX_MIN.z);
            ImGui::Text("Box Max: (%.2f, %.2f, %.2f)", BOX_MAX.x, BOX_MAX.y, BOX_MAX.z);
            ImGui::Text("PI Value: %.2f", M_PI);

            static float smoothingRadius = SMOOTHING_RADIUS;
            static float particleMass = PARTICLE_MASS;
            static float restDensity = REST_DENSITY;
            static float pressureConstant = PRESSURE_CONSTANT;
            static float viscosityConstant = VISCOSITY_CONSTANT;
            static float gravity = GRAVITY;
            static float bounceDamping = BOUNCE_DAMPING;
            static float timeStep = TIME_STEP;

            if (ImGui::DragFloat("Smoothing Radius", &smoothingRadius, 0.001f, 0.01f, 100.0f, "%.4f"))
                SMOOTHING_RADIUS = smoothingRadius;
            if (ImGui::DragFloat("Particle Mass", &particleMass, 0.01f, 0.001f, 100.0f, "%.4f"))
                PARTICLE_MASS = particleMass;
            if (ImGui::DragFloat("Rest Density", &restDensity, 10.0f, 100.0f, 5000.0f, "%.1f"))
                REST_DENSITY = restDensity;
            if (ImGui::DragFloat("Pressure Constant", &pressureConstant, 1.0f, 0.0f, 1000.0f, "%.1f"))
                PRESSURE_CONSTANT = pressureConstant;
            if (ImGui::DragFloat("Viscosity Constant", &viscosityConstant, 0.001f, 0.0f, 1.0f, "%.4f"))
                VISCOSITY_CONSTANT = viscosityConstant;
            if (ImGui::DragFloat("Gravity", &gravity, 0.1f, -20.0f, 0.0f, "%.2f"))
                GRAVITY = gravity;
            if (ImGui::DragFloat("Bounce Damping", &bounceDamping, 0.01f, 0.0f, 1.0f, "%.2f"))
                BOUNCE_DAMPING = bounceDamping;
            if (ImGui::DragFloat("Time Step", &timeStep, 0.0001f, 0.0001f, 0.1f, "%.4f"))
                TIME_STEP = timeStep;

            if (ImGui::Button("Reset to Defaults")) {
                smoothingRadius = SMOOTHING_RADIUS;
                particleMass = PARTICLE_MASS;
                restDensity = REST_DENSITY;
                pressureConstant = PRESSURE_CONSTANT;
                viscosityConstant = VISCOSITY_CONSTANT;
                gravity = GRAVITY;
                bounceDamping = BOUNCE_DAMPING;
                timeStep = TIME_STEP;
            }
            ImGui::Separator();
            ImGui::Text("Simulation Controls");
            static bool isPaused = false;
            if (ImGui::Button(isPaused ? "Resume" : "Pause")) isPaused = !isPaused;
            ImGui::SameLine();
            if (ImGui::Button("Reset Simulation")) { /* TODO */ }
            ImGui::Separator();
            ImGui::Text("Performance");
            ImGui::Text("Particles: %d", PARTICLE_COUNT);
        }

        if (ImGui::CollapsingHeader("Model Controls")) {
            ImGui::Text("Model Position");
            ImGui::DragFloat3("Position", scene.modelPosition, 0.01f, -10.0f, 10.0f);
            ImGui::Text("Model Rotation");
            ImGui::DragFloat3("Rotation (degrees)", scene.modelRotation, 1.0f, -180.0f, 180.0f);
        }

        if (ImGui::CollapsingHeader("Camera Controls")) {
            ImGui::Text("Camera Position");
            ImGui::DragFloat3("Camera Pos", &scene.camera.position.x, 0.01f, -10.0f, 10.0f);
            ImGui::Text("Camera Rotation");
            if (ImGui::DragFloat("Yaw (degrees)", &cameraYawDeg, 1.0f, -180.0f, 180.0f))
                scene.camera.yaw = cameraYawDeg * (pi / 180.0f);
            if (ImGui::Button("Reset Camera")) {
                scene.camera.position = {0.0f, 0.0f, 0.0f};
                scene.camera.yaw = 0.0f;
            }
        }

        if (ImGui::CollapsingHeader("Lighting")) {
            ImGui::ColorEdit3("Light Color", scene.lightColor);
            ImGui::Text("Light Direction");
            ImGui::DragFloat3("Direction", scene.lightDirection, 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat("specular strength", &scene.specularStrength, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("ambient strength", &scene.ambientStrength, 0.01f, 0.0f, 1.0f);
            ImGui::Text("Visualization");
            ImGui::ColorEdit3("Background Color", scene.backgroundColor);
        }

        if (ImGui::CollapsingHeader("Statistics")) {
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);
            ImGui::Text("Camera Yaw: %.2f degrees", scene.camera.yaw * 180.0f / pi);
        }

        if (ImGui::CollapsingHeader("Help")) {
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
        ImGui::Checkbox("Show Demo Window", &scene.showDemoWindow);
        ImGui::End();
    }
    ImGui::Render();
}
