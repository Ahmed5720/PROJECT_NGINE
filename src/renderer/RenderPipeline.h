#pragma once
#include "Scene.h"
#include "EditorUI.h"
#include "miniVM.h"
#include "geometry.h"
#include "particleSimulation.h"
#include "ParticleRenderer.h"
#include "shader.h"
#include <vector>

class RenderPipeline {
public:
    bool drawBoundingBox = false;
    RenderPipeline(shader* phongShader, shader* wireframeShader, ParticleRenderer* particleRenderer,
    shader* skyBoxShader, shader* shadowShader);
    ~RenderPipeline();

    void render(Scene& scene, int framebufferW, int framebufferH, float zNear, float zFar);
    bool takeShootRequest();

private:
    void renderShadowPass(Scene& scene, const mat4x4& view, const mat4x4& projection);
    void renderLightingPass(Scene& scene, const mat4x4& view, const mat4x4& projection, const mat4x4& lightSpace);
    void renderWireframePass(Scene& scene, const mat4x4& view, const mat4x4& projection);
    void renderSkyBoxPass(Scene& scene, const mat4x4& view, const mat4x4& projection);
    void uploadLighting(shader& s, const LightEnvironment& lights);
    void resizeSceneFramebuffer(int w, int h);
    void destroySceneFramebuffer();
    GLuint getWhiteTex();
    WireFrameMesh wireFrameMesh_;
    shader* wireFrameShader_ = nullptr;
    shader* phongShader_ = nullptr;
    shader* skyBoxShader_ = nullptr;
    shader* shadowShader_ = nullptr;
    ParticleRenderer* particleRenderer_ = nullptr;
    EditorUI editorUI_;

    GLuint sceneFbo_ = 0;
    GLuint depthMapFbo_ = 0;
    GLuint depthMap;
    GLuint sceneColorTex_ = 0;
    GLuint sceneDepthRbo_ = 0;
    GLuint whiteTex_ = 0;
    int sceneFbW_ = 0;
    int sceneFbH_ = 0;
    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024; // should be able to tweek in config
};
