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

RenderPipeline::~RenderPipeline() {
    destroySceneFramebuffer();
}

void RenderPipeline::resizeSceneFramebuffer(int w, int h) {
    if (w < 8 || h < 8)
        return;
    if (w == sceneFbW_ && h == sceneFbH_)
        return;

    destroySceneFramebuffer();
    sceneFbW_ = w;
    sceneFbH_ = h;

    glGenFramebuffers(1, &sceneFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);

    glGenTextures(1, &sceneColorTex_);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTex_, 0);

    glGenRenderbuffers(1, &sceneDepthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthRbo_);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[RenderPipeline] Scene framebuffer incomplete: 0x" << std::hex << status << std::dec << "\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::destroySceneFramebuffer() {
    if (sceneDepthRbo_) {
        glDeleteRenderbuffers(1, &sceneDepthRbo_);
        sceneDepthRbo_ = 0;
    }
    if (sceneColorTex_) {
        glDeleteTextures(1, &sceneColorTex_);
        sceneColorTex_ = 0;
    }
    if (sceneFbo_) {
        glDeleteFramebuffers(1, &sceneFbo_);
        sceneFbo_ = 0;
    }
    sceneFbW_ = 0;
    sceneFbH_ = 0;
}

void RenderPipeline::render(Scene& scene, int framebufferW, int framebufferH,
                            float zNear, float zFar) {
    const EditorUI::Layout layout = editorUI_.computeLayout(framebufferW, framebufferH);

    if (layout.sceneW >= 8 && layout.sceneH >= 8) {
        resizeSceneFramebuffer(layout.sceneW, layout.sceneH);

        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
        glViewport(0, 0, layout.sceneW, layout.sceneH);
        glEnable(GL_DEPTH_TEST);
        glClearColor(scene.backgroundColor[0], scene.backgroundColor[1], scene.backgroundColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float aspect = static_cast<float>(layout.sceneW) / static_cast<float>(layout.sceneH);
        const mat4x4 view = scene.camera.getViewMatrix();
        const mat4x4 projection = scene.camera.getProjectionMatrix(aspect, zNear, zFar);
        renderPhongPass(scene, view, projection);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glViewport(0, 0, framebufferW, framebufferH);
    glClearColor(0.06f, 0.06f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    editorUI_.beginFrame();
    editorUI_.draw(scene, sceneColorTex_, layout);
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
