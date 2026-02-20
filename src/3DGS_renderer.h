/*
    Rasterizes a set of Gaussians loaded from a PLY file.
    Each Gaussian is rasterized as a quad. Vertex shader computes 3D covariance from
    pos/rot/scale, projects to 2D, then quad axes/radius. CPU sort (back-to-front) is
    used for correct alpha blending. Fragment shader uses opacity + SH coefficients.
*/
#pragma once
#include "miniVM.h"
#include <glad/glad.h>
#include <vector>
#include "shader.h"
#include "geometry.h"
#include <cmath>
#include <string>


class GaussianRenderer
{
public:
    GaussianRenderer();
    ~GaussianRenderer();

    void init();
    void init(const std::string& vertexPath, const std::string& fragmentPath);
    // Renders Gaussians in sorted order. view/proj in column-major (same as miniVM).
    // viewportW/H and fovDeg are used for u_viewport and u_focal.
    void render(const std::vector<Gaussian>& gaussians,
                const std::vector<int>& sorted_indices,
                const mat4x4& view,
                const mat4x4& proj,
                int viewportW,
                int viewportH,
                float fovDeg);

private:
    GLuint VAO = 0;
    GLuint VBO = 0;
    shader* gaussianShader = nullptr;
    std::vector<GaussianGPU> m_gpu_buffer;
};

// Build a GaussianGPU from a Gaussian (applies exp/sigmoid activations).
static GaussianGPU to_gpu(const Gaussian& g) {
    GaussianGPU gpu;

    // Opacity stays as logit — the vertex shader applies sigmoid
    gpu.position_opacity = { g.pos.x, g.pos.y, g.pos.z, g.opacity };

    // Rotation quaternion passthrough
    gpu.rot = g.rot;

    // Scale stays as log — the vertex shader applies exp
    gpu.scale = { g.scale.x, g.scale.y , g.scale.z, 0.0f };

    // Pack SH coefficients into vec4s
    // g.sh layout: [R0..R3, G0..G3, B0..B3]
    gpu.sh[0] = { g.sh[0], g.sh[1], g.sh[2], g.sh[3] };   // R channel
    gpu.sh[1] = { g.sh[4], g.sh[5], g.sh[6], g.sh[7] };   // G channel
    gpu.sh[2] = { g.sh[8], g.sh[9], g.sh[10], g.sh[11] }; // B channel

    return gpu;
}


inline GaussianRenderer::GaussianRenderer() = default;

inline GaussianRenderer::~GaussianRenderer() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    delete gaussianShader;
}

inline void GaussianRenderer::init() {
    init("shaders/gaussian.vs", "shaders/gaussian.fs");
}

inline void GaussianRenderer::init(const std::string& vertexPath, const std::string& fragmentPath) {
    gaussianShader = new shader(vertexPath.c_str(), fragmentPath.c_str());
    // --- Create VAO and VBO ---
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Each attribute is per-instance (one GaussianGPU per Gaussian, not per vertex).
    // We use glVertexAttribDivisor(attr, 1) to advance the attribute pointer once
    // per instance rather than once per vertex.

    const GLsizei stride = sizeof(GaussianGPU);

    // location 0: position_opacity (vec4)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride,
        (void*)offsetof(GaussianGPU, position_opacity));
    glVertexAttribDivisor(0, 1);

    // location 1: rotation (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
        (void*)offsetof(GaussianGPU, rot));
    glVertexAttribDivisor(1, 1);

    // location 2: scale (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
        (void*)offsetof(GaussianGPU, scale));
    glVertexAttribDivisor(2, 1);

    // location 3,4,5: sh[0], sh[1], sh[2] (vec4 each)
    for (int i = 0; i < 3; ++i) {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, stride,
            (void*)(offsetof(GaussianGPU, sh) + i * sizeof(vec4f)));
        glVertexAttribDivisor(3 + i, 1);
    }

    glBindVertexArray(0);
    //glEnableVertexArray(0);
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // glDisable(GL_DEPTH_TEST);
}

inline void GaussianRenderer::render(const std::vector<Gaussian>& gaussians,
                                    const std::vector<int>& sorted_indices,
                                    const mat4x4& view,
                                    const mat4x4& proj,
                                    int viewportW,
                                    int viewportH,
                                    float fovDeg) {
    const int n = static_cast<int>(sorted_indices.size());
    if (n == 0 || !gaussianShader || gaussianShader->ID == 0) return;

    // Build sorted GPU buffer
    m_gpu_buffer.resize(n);
    for (int i = 0; i < n; ++i) {
        m_gpu_buffer[i] = to_gpu(gaussians[sorted_indices[i]]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 n * sizeof(GaussianGPU),
                 m_gpu_buffer.data(),
                 GL_DYNAMIC_DRAW);

    gaussianShader->use();
    GLuint prog = gaussianShader->ID;

    // Upload view/projection in row-major order
    float viewMat[16] = {
        view.m[0][0], view.m[0][1], view.m[0][2], view.m[0][3],
        view.m[1][0], view.m[1][1], view.m[1][2], view.m[1][3],
        view.m[2][0], view.m[2][1], view.m[2][2], view.m[2][3],
        view.m[3][0], view.m[3][1], view.m[3][2], view.m[3][3]
    };

    float projMat[16] = {
        proj.m[0][0], proj.m[0][1], proj.m[0][2], proj.m[0][3],
        proj.m[1][0], proj.m[1][1], proj.m[1][2], proj.m[1][3],
        proj.m[2][0], proj.m[2][1], proj.m[2][2], proj.m[2][3],
        proj.m[3][0], proj.m[3][1], proj.m[3][2], proj.m[3][3]
    };
    glUniformMatrix4fv(glGetUniformLocation(prog, "u_view"), 1, GL_FALSE, viewMat);
    glUniformMatrix4fv(glGetUniformLocation(prog, "u_proj"), 1, GL_FALSE, projMat);

    // Focal length from FOV and viewport (same convention as projection)
    float fovRad = fovDeg * (3.14159265f / 180.0f);
    float tanHalfFov = std::tan(fovRad * 0.5f);
    float focalX = (float)viewportW / (2.0f * tanHalfFov);
    float focalY = (float)viewportH / (2.0f * tanHalfFov);
    glUniform2f(glGetUniformLocation(prog, "u_focal"), focalX, focalY);
    glUniform2f(glGetUniformLocation(prog, "u_viewport"), (float)viewportW, (float)viewportH);

    glBindVertexArray(VAO);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n);
    glBindVertexArray(0);
}


