#pragma once
#include "Camera.h"
#include "geometry.h"
#include <vector>

struct Scene {
    Camera camera;

    std::vector<MeshGPU> meshes;
    //GLuint textureId = 0;

    std::vector<Gaussian> gaussians;

    float modelPosition[3] = {0.0f, 0.0f, 1.0f};
    float modelRotation[3] = {0.0f, 0.0f, 0.0f};

    float lightDirection[3] = {0.0f, 0.0f, 1.0f};
    float lightColor[3]      = {1.0f, 1.0f, 1.0f};
    float backgroundColor[3] = {0.2f, 0.5f, 0.5f};
    float specularStrength = 0.8f;
    float ambientStrength = 0.2f;

    //bool showDemoWindow   = false;
    bool showControlWindow = true;

    void destroy() {
        for (auto &mesh : meshes)
            mesh.destroy();
    }
};
