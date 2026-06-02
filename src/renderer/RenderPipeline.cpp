#include "RenderPipeline.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <iostream>
#include <string>

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

// Build a uniform name like "pointLights[2].diffuse" without heap allocation.
// `buf` must be large enough (64 bytes is plenty).
void fmtUniform(char* buf, size_t sz, const char* array, int idx, const char* field) {
    snprintf(buf, sz, "%s[%d].%s", array, idx, field);
}

void checkGLError(const char* ctx) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        std::cerr << "GL error in " << ctx << ": 0x" << std::hex << err << std::dec << "\n";
}
} // namespace

RenderPipeline::RenderPipeline(shader* phongShader, ParticleRenderer* particleRenderer)
    : phongShader_(phongShader), particleRenderer_(particleRenderer) {}

void RenderPipeline::render(Scene& scene, SPHSimulator& simulator, int viewportW, int viewportH, float zNear,  float zFar) {
    glClearColor(scene.backgroundColor[0], scene.backgroundColor[1], scene.backgroundColor[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float  aspect     = static_cast<float>(viewportW) / static_cast<float>(viewportH);
    mat4x4 view       = scene.camera.getViewMatrix();
    mat4x4 projection = scene.camera.getProjectionMatrix(aspect, zNear, zFar);

    renderPhongPass(scene, view, projection);
    //renderImGui(scene, PI, simulator);
    renderImGui(scene, PI); 

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RenderPipeline::uploadLighting(shader& s, const LightEnvironment& lights) {
    // Directional light
    const DirectionalLight& sun = lights.sun;
    s.setFloat3("dirLight.direction", sun.direction[0], sun.direction[1], sun.direction[2]);
    s.setFloat3("dirLight.ambient",   sun.ambient[0],   sun.ambient[1],   sun.ambient[2]);
    s.setFloat3("dirLight.diffuse",   sun.diffuse[0],   sun.diffuse[1],   sun.diffuse[2]);
    s.setFloat3("dirLight.specular",  sun.specular[0],  sun.specular[1],  sun.specular[2]);

    // Point lights
    // Only upload up to numPointLights; the shader uses `numPointLights` to
    // bound its loop so unset array slots are never read.
    char buf[64];
    int activePoints = 0;
    for (int i = 0; i < lights.numPointLights; ++i) {
        const PointLight& pl = lights.pointLights[i];
        if (!pl.enabled) continue;

        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "position");
        s.setFloat3(buf, pl.position[0], pl.position[1], pl.position[2]);

        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "constant");
        s.setFloat(buf, pl.constant);
        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "linear");
        s.setFloat(buf, pl.linear);
        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "quadratic");
        s.setFloat(buf, pl.quadratic);

        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "ambient");
        s.setFloat3(buf, pl.ambient[0],  pl.ambient[1],  pl.ambient[2]);
        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "diffuse");
        s.setFloat3(buf, pl.diffuse[0],  pl.diffuse[1],  pl.diffuse[2]);
        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "specular");
        s.setFloat3(buf, pl.specular[0], pl.specular[1], pl.specular[2]);

        ++activePoints;
    }
    s.setInt("numPointLights", activePoints);
    // Spot light — shader uses a single `spotLight` uniform (first enabled only)
    int activeSpots = 0;
    for (int i = 0; i < lights.numSpotLights && activeSpots < 1; ++i) {
        const SpotLight& sl = lights.spotLights[i];
        if (!sl.enabled) continue;

        s.setFloat3("spotLight.position",  sl.position[0],  sl.position[1],  sl.position[2]);
        s.setFloat3("spotLight.direction", sl.direction[0], sl.direction[1], sl.direction[2]);
        s.setFloat("spotLight.cutOff",      sl.cutOff);
        s.setFloat("spotLight.outerCutOff", sl.outerCutOff);
        s.setFloat("spotLight.constant",   sl.constant);
        s.setFloat("spotLight.linear",     sl.linear);
        s.setFloat("spotLight.quadratic",  sl.quadratic);
        s.setFloat3("spotLight.ambient",  sl.ambient[0],  sl.ambient[1],  sl.ambient[2]);
        s.setFloat3("spotLight.diffuse",  sl.diffuse[0],  sl.diffuse[1],  sl.diffuse[2]);
        s.setFloat3("spotLight.specular", sl.specular[0], sl.specular[1], sl.specular[2]);

        activeSpots = 1;
    }
    s.setInt("numSpotLights", activeSpots);
}

// renderPhongPass
//   1. Upload per-frame uniforms: matrices, camera, all lights.
//   2. For each visible SceneNode: upload model matrix + material, draw.

//   We bind the diffuse map to unit 0, specular map to unit 1.
//   If a map is missing we bind a 1×1 white fallback (see getWhiteTex).
void RenderPipeline::renderPhongPass(Scene& scene, const mat4x4& view, const mat4x4& projection) {
    phongShader_->use();

    // Per-frame: matrices
    setMat4(*phongShader_, "view",       view);
    setMat4(*phongShader_, "projection", projection);

    // Per-frame: camera position
    phongShader_->setFloat3("viewPos", scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);

    // Per-frame: all lights
    uploadLighting(*phongShader_, scene.lights);

    // Bind sampler uniforms to their fixed texture units (set once per frame)
    phongShader_->setInt("material.diffuse",  0);   // GL_TEXTURE0
    phongShader_->setInt("material.specular", 1);   // GL_TEXTURE1

    // Per-node draw
    for (const SceneNode& node : scene.nodes) {
        if (!node.visible)                continue;
        if (node.meshIndex == -1) continue;
        if (node.meshIndex >= static_cast<int>(scene.meshes.size())) continue;

        // Model matrix
        mat4x4 model = node.modelMatrix();
        setMat4(*phongShader_, "model", model);

        // material.shininess
        phongShader_->setFloat("material.shininess", node.material.shininess);

        // Diffuse map
        glActiveTexture(GL_TEXTURE0);
        if (node.material.diffuseMap.valid())
            glBindTexture(GL_TEXTURE_2D, node.material.diffuseMap.id);
        else
        {
            //std::cout << "invalid material, using white texture instead\n";
            glBindTexture(GL_TEXTURE_2D, getWhiteTex());
        }

        // Specular map
        // If no dedicated specular map, reuse the diffuse map
        glActiveTexture(GL_TEXTURE1);
        if (node.material.specularMap.valid())
            glBindTexture(GL_TEXTURE_2D, node.material.specularMap.id);
        else if (node.material.diffuseMap.valid())
            glBindTexture(GL_TEXTURE_2D, node.material.diffuseMap.id);
        else
            glBindTexture(GL_TEXTURE_2D, getWhiteTex());

        scene.meshes[node.meshIndex].draw();
    }

    checkGLError("renderPhongPass");
}

// getWhiteTex
//   Lazy-initialises a 1×1 white RGBA texture used as a fallback when a
//   node has no diffuse or specular map. 
GLuint RenderPipeline::getWhiteTex() {
    if (whiteTex_ != 0) return whiteTex_;
    unsigned char white[4] = {255, 255, 255, 255};
    glGenTextures(1, &whiteTex_);
    glBindTexture(GL_TEXTURE_2D, whiteTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return whiteTex_;
}

// renderImGui
void RenderPipeline::renderImGui(Scene& scene, float pi) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    float cameraYawDeg = scene.camera.yaw * (180.0f / pi);

    if (scene.showControlWindow) {
        ImGui::Begin("Controls", &scene.showControlWindow);

        //  SPH 
        if (ImGui::CollapsingHeader("SPH Simulation Parameters")) {
            ImGui::Text("Particle Count: %d", PARTICLE_COUNT);
            static float smoothingRadius   = SMOOTHING_RADIUS;
            static float particleMass      = PARTICLE_MASS;
            static float restDensity       = REST_DENSITY;
            static float pressureConstant  = PRESSURE_CONSTANT;
            static float viscosityConstant = VISCOSITY_CONSTANT;
            static float gravity           = GRAVITY;
            static float bounceDamping     = BOUNCE_DAMPING;
            static float timeStep          = TIME_STEP;
            if (ImGui::DragFloat("Smoothing Radius",   &smoothingRadius,   0.001f, 0.01f,   100.0f, "%.4f")) SMOOTHING_RADIUS   = smoothingRadius;
            if (ImGui::DragFloat("Particle Mass",      &particleMass,      0.01f,  0.001f,  100.0f, "%.4f")) PARTICLE_MASS      = particleMass;
            if (ImGui::DragFloat("Rest Density",       &restDensity,       10.0f,  100.0f, 5000.0f, "%.1f")) REST_DENSITY       = restDensity;
            if (ImGui::DragFloat("Pressure Constant",  &pressureConstant,  1.0f,   0.0f,  1000.0f, "%.1f")) PRESSURE_CONSTANT  = pressureConstant;
            if (ImGui::DragFloat("Viscosity Constant", &viscosityConstant, 0.001f, 0.0f,    1.0f,  "%.4f")) VISCOSITY_CONSTANT = viscosityConstant;
            if (ImGui::DragFloat("Gravity",            &gravity,           0.1f,  -20.0f,   0.0f,  "%.2f")) GRAVITY            = gravity;
            if (ImGui::DragFloat("Bounce Damping",     &bounceDamping,     0.01f,  0.0f,    1.0f,  "%.2f")) BOUNCE_DAMPING     = bounceDamping;
            if (ImGui::DragFloat("Time Step",          &timeStep,          0.0001f,0.0001f, 0.1f,  "%.4f")) TIME_STEP          = timeStep;
        }

        // Scene Nodes
        if (ImGui::CollapsingHeader("Scene Nodes")) {
            for (int i = 0; i < static_cast<int>(scene.nodes.size()); ++i) {
                SceneNode& node = scene.nodes[i];
                std::string label = node.name.empty() ? ("Node " + std::to_string(i)) : node.name;
                if (ImGui::TreeNode(label.c_str())) {
                    ImGui::Checkbox("Visible",      &node.visible);
                    ImGui::Checkbox("Casts Shadow", &node.castsShadow);
                    ImGui::Checkbox("Recv Shadow",  &node.receivesShadow);
                    ImGui::Separator();
                    ImGui::Text("Transform");
                    ImGui::DragFloat3("Position##n", node.position, 0.01f, -100.0f, 100.0f);
                    ImGui::DragFloat3("Rotation##n", node.rotation, 1.0f,  -180.0f, 180.0f);
                    ImGui::DragFloat3("Scale##n",    node.scale,    0.01f,    0.01f, 100.0f);
                    ImGui::Separator();
                    ImGui::Text("Material: %s", node.material.name.c_str());
                    ImGui::DragFloat("Shininess", &node.material.shininess, 1.0f, 1.0f, 256.0f);
                    ImGui::Text("Diffuse map:  %s", node.material.diffuseMap.valid()  ? "loaded" : "none (white fallback)");
                    ImGui::Text("Specular map: %s", node.material.specularMap.valid() ? "loaded" : "none (diffuse fallback)");
                    ImGui::TreePop();
                }
            }
        }

        // Camera
        if (ImGui::CollapsingHeader("Camera Controls")) {
            ImGui::DragFloat3("Camera Pos", &scene.camera.position.x, 0.01f, -100.0f, 100.0f);
            if (ImGui::DragFloat("Yaw (degrees)", &cameraYawDeg, 1.0f, -180.0f, 180.0f))
                scene.camera.yaw = cameraYawDeg * (pi / 180.0f);
            if (ImGui::Button("Reset Camera")) {
                scene.camera.position = {0.0f, 0.0f, 0.0f};
                scene.camera.yaw = 0.0f;
            }
        }

        // Lighting
        if (ImGui::CollapsingHeader("Lighting")) {
            ImGui::Text("Directional Light (Sun)");
            ImGui::DragFloat3("Sun Direction", scene.lights.sun.direction, 0.01f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Sun Ambient",   scene.lights.sun.ambient);
            ImGui::ColorEdit3("Sun Diffuse",   scene.lights.sun.diffuse);
            ImGui::ColorEdit3("Sun Specular",  scene.lights.sun.specular);

            ImGui::Separator();
            ImGui::Text("Point Lights  (%d / %d active)", scene.lights.numPointLights, MAX_POINT_LIGHTS);
            for (int i = 0; i < scene.lights.numPointLights; ++i) {
                PointLight& pl = scene.lights.pointLights[i];
                std::string tag = "Point Light " + std::to_string(i);
                if (ImGui::TreeNode(tag.c_str())) {
                    ImGui::Checkbox("Enabled##pl",      &pl.enabled);
                    ImGui::DragFloat3("Position##pl",    pl.position, 0.05f, -100.0f, 100.0f);
                    ImGui::ColorEdit3("Ambient##pl",     pl.ambient);
                    ImGui::ColorEdit3("Diffuse##pl",     pl.diffuse);
                    ImGui::ColorEdit3("Specular##pl",    pl.specular);
                    ImGui::DragFloat("Constant##pl",    &pl.constant,  0.001f, 0.0f, 2.0f,  "%.4f");
                    ImGui::DragFloat("Linear##pl",      &pl.linear,    0.001f, 0.0f, 1.0f,  "%.4f");
                    ImGui::DragFloat("Quadratic##pl",   &pl.quadratic, 0.001f, 0.0f, 0.5f,  "%.4f");
                    ImGui::TreePop();
                }
            }
            if (scene.lights.numPointLights < MAX_POINT_LIGHTS) {
                if (ImGui::Button("Add Point Light"))
                    scene.lights.addPointLight(PointLight{});
            }

            ImGui::Separator();
            ImGui::Text("Spot Lights  (%d / %d active)", scene.lights.numSpotLights, MAX_SPOT_LIGHTS);
            for (int i = 0; i < scene.lights.numSpotLights; ++i) {
                SpotLight& sl = scene.lights.spotLights[i];
                std::string tag = "Spot Light " + std::to_string(i);
                if (ImGui::TreeNode(tag.c_str())) {
                    ImGui::Checkbox("Enabled##sl",      &sl.enabled);
                    ImGui::DragFloat3("Position##sl",    sl.position,  0.05f, -100.0f, 100.0f);
                    ImGui::DragFloat3("Direction##sl",   sl.direction, 0.01f,   -1.0f,   1.0f);
                    ImGui::ColorEdit3("Ambient##sl",     sl.ambient);
                    ImGui::ColorEdit3("Diffuse##sl",     sl.diffuse);
                    ImGui::ColorEdit3("Specular##sl",    sl.specular);
                    ImGui::DragFloat("Constant##sl",    &sl.constant,  0.001f, 0.0f, 2.0f,  "%.4f");
                    ImGui::DragFloat("Linear##sl",      &sl.linear,    0.001f, 0.0f, 1.0f,  "%.4f");
                    ImGui::DragFloat("Quadratic##sl",   &sl.quadratic, 0.001f, 0.0f, 0.5f,  "%.4f");
                    // Show angles in degrees; store as cosines
                    float innerDeg = acosf(sl.cutOff)      * (180.0f / pi);
                    float outerDeg = acosf(sl.outerCutOff) * (180.0f / pi);
                    if (ImGui::DragFloat("Inner Angle##sl", &innerDeg, 0.5f, 1.0f, 45.0f))
                        sl.cutOff      = cosf(innerDeg * (pi / 180.0f));
                    if (ImGui::DragFloat("Outer Angle##sl", &outerDeg, 0.5f, 1.0f, 60.0f))
                        sl.outerCutOff = cosf(outerDeg * (pi / 180.0f));
                    ImGui::TreePop();
                }
            }
            if (scene.lights.numSpotLights < MAX_SPOT_LIGHTS) {
                if (ImGui::Button("Add Spot Light"))
                    scene.lights.addSpotLight(SpotLight{});
            }

            ImGui::Separator();
            ImGui::ColorEdit3("Background", scene.backgroundColor);
        }

        // Stats
        if (ImGui::CollapsingHeader("Statistics")) {
            ImGui::Text("%.3f ms/frame  (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Text("Camera (%.2f, %.2f, %.2f)  Yaw %.1f°",
                scene.camera.position.x, scene.camera.position.y, scene.camera.position.z,
                scene.camera.yaw * 180.0f / pi);
            ImGui::Text("Nodes: %d   Meshes: %d   PointLights: %d   SpotLights: %d",
                (int)scene.nodes.size(), (int)scene.meshes.size(),
                scene.lights.numPointLights, scene.lights.numSpotLights);
        }

        if (ImGui::CollapsingHeader("Help")) {
            ImGui::BulletText("W/S: camera up/down");
            ImGui::BulletText("A/D: camera left/right");
            ImGui::BulletText("Up/Down Arrow: move forward/back");
            ImGui::BulletText("Left/Right Arrow: rotate camera");
        }

        ImGui::Separator();
        ImGui::End();
    }
    ImGui::Render();
}