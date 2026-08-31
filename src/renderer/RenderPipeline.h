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
    RenderPipeline(unique_ptr<shader> pbrShader, unique_ptr<shader> wireframeShader,
    unique_ptr<ParticleRenderer> particleRenderer,
    unique_ptr<shader> skyBoxShader, unique_ptr<shader> shadowShader,
    unique_ptr<shader> hdrCaptureShader, unique_ptr<shader> prefilter,
    unique_ptr<shader> brdf, unique_ptr<shader> convolve);
    ~RenderPipeline();

    void render(Scene& scene, int framebufferW, int framebufferH, float zNear, float zFar);
    bool takeShootRequest();

private:
    void captureHdrCubeMap(Scene& scene);
    void renderCube();
    void renderQuad();
    void renderNode(const Scene& scene, const SceneNode& node); // shader* pbrShader_);
    void renderShadowPass(Scene& scene, const mat4x4& view, const mat4x4& projection);
    void renderLightingPass(Scene& scene, const mat4x4& view, const mat4x4& projection, const mat4x4& lightSpace);
    void renderWireframePass(Scene& scene, const mat4x4& view, const mat4x4& projection);
    void renderSkyBoxPass(Scene& scene, const mat4x4& view, const mat4x4& projection);
    void uploadLighting(shader& s, const LightEnvironment& lights);
    void resizeSceneFramebuffer(int w, int h, Scene& scene);
    void prefilterSpecularCubemap(Scene& scene);
    void convolveHDRCubeMap(Scene& scene);
    void genBrdfLUT();
    void destroySceneFramebuffer();
    GLuint getWhiteTex();
    WireFrameMesh wireFrameMesh_;
    unique_ptr<shader> wireFrameShader_ = nullptr;
    unique_ptr<shader> phongShader_ = nullptr;
    unique_ptr<shader> skyBoxShader_ = nullptr;
    unique_ptr<shader> shadowShader_ = nullptr;
    unique_ptr<shader> pbrShader_ = nullptr;
    unique_ptr<shader> captureHdrShader_ = nullptr;
    unique_ptr<shader> prefilterShader_ = nullptr;
    unique_ptr<shader> brdfShader_ = nullptr;
    unique_ptr<shader> convolveShader_ = nullptr;
    unique_ptr<ParticleRenderer> particleRenderer_ = nullptr;
    EditorUI editorUI_;

    GLuint sceneFbo_ = 0;
    GLuint depthMapFbo_ = 0;
    GLuint depthMap;
    GLuint sceneColorTex_ = 0;
    GLuint sceneDepthRbo_ = 0;
    GLuint captureFBO;
    GLuint captureRBO;
    GLuint brdfLUT;
    GLuint irradianceMap;
    GLuint whiteTex_ = 0;
    mat4x4 captureProjection;
    mat4x4 captureViews[6];
    int sceneFbW_ = 0;
    int sceneFbH_ = 0;
    bool hasValidBuffers = false;
    bool generatedLUT = false;
    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024; // should be able to tweek in config
};
