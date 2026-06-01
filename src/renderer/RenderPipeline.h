#pragma once
#include "Scene.h"
#include "miniVM.h"
#include "geometry.h"
#include "particleSimulation.h"
#include "ParticleRenderer.h"
#include "shader.h"
#include <vector>

class RenderPipeline {
public:
    RenderPipeline(shader* phongShader,ParticleRenderer* particleRenderer);
    void render(Scene& scene, SPHSimulator& simulator, int viewportW, int viewportH, float zNear, float zFar);
private:
    void renderPhongPass(Scene& scene, const mat4x4& view, const mat4x4& projection);
    void renderImGui(Scene& scene, float pi, SPHSimulator& simulator);
    void uploadLighting(shader& s, const LightEnvironment& lights);
    GLuint getWhiteTex();
    shader* phongShader_ = nullptr;
    ParticleRenderer* particleRenderer_ = nullptr;
    GLuint whiteTex_ = 0; 
};
