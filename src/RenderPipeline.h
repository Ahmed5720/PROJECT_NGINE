#pragma once
#include "Scene.h"
#include "miniVM.h"
#include "geometry.h"
#include "sort.h"
#include "particleSimulation.h"
#include "3DGS_renderer.h"
#include "ParticleRenderer.h"
#include "shader.h"
#include <vector>

class RenderPipeline {
public:
    RenderPipeline(shader* phongShader,
                   GaussianRenderer* gaussianRenderer,
                   ParticleRenderer* particleRenderer);

    void render(Scene& scene,
                SPHSimulator& simulator,
                int viewportW,
                int viewportH,
                float zNear,
                float zFar);

private:
    void renderImGui(Scene& scene, float pi);

    shader* phongShader_ = nullptr;
    GaussianRenderer* gaussianRenderer_ = nullptr;
    ParticleRenderer* particleRenderer_ = nullptr;
};
