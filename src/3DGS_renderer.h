/*
    rasterizes a set of gaussians loaded as a ply file

    each gaussian is rasterized as a quad
    vertex shader performs projection first computes 3d covariance from pos/rot/scale data stored in the buffer then projects to 2d covariance then we need to compute eigenvectors
    to get quad axes and radius.
    A CPU sort is performed std sort which is a quick sort-ish implementation. this is important so the fragment shader can perform correct alpha blending (back to front)
    fragment shader determines alpha/colors based on alpha + sh coefficients
*/
pragma once
#include "miniVM.h"
#include <glad/glad.h>
#include <vector>
#include "shader.h"
#include "geometry.h"

using mat3f = mat4x4; // haha
using mat2f = mat4x4;


class GaussianRenderer
{
    public: 
        GaussianRenderer();
        ~GaussianRenderer();

        void init();
        void render(vector<Gaussian>& Gaussians, vector<int>& sorted_indices, int campos);
    private:
        GLuint VAO;
        GLuint VBO;
        shader* gaussianShader; 
        int m_width  = 0;
        int m_height = 0;
        // Scratch buffer reused each frame to avoid re-allocating
        vector<GaussianGPU> m_gpu_buffer; 
};

// Build a GaussianGPU from a Gaussian (applies exp/sigmoid activations).
static GaussianGPU to_gpu(const Gaussian& g) {
    GaussianGPU gpu;

    // Opacity stays as logit — the vertex shader applies sigmoid
    gpu.position_opacity = { g.pos.x, g.pos.y, g.pos.z, g.opacity };

    // Rotation quaternion passthrough
    gpu.rot = g.rot;

    // Scale stays as log — the vertex shader applies exp
    gpu.scale = { g.scale.x, g.scale.y, g.scale.z, 0.0f };

    // Pack SH coefficients into vec4s
    // g.sh layout: [R0..R3, G0..G3, B0..B3]
    gpu.sh[0] = { g.sh[0], g.sh[1], g.sh[2], g.sh[3] };   // R channel
    gpu.sh[1] = { g.sh[4], g.sh[5], g.sh[6], g.sh[7] };   // G channel
    gpu.sh[2] = { g.sh[8], g.sh[9], g.sh[10], g.sh[11] }; // B channel

    return gpu;
}


GaussianRenderer::~GaussianRenderer()
{
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    delete gaussianShader;
}

void GaussianRenderer::init()
{
    // create shaders
    // bind vaos
    gaussianShader = new shader(
        "C:\\Dev\\git\\PROJECT_NGINE\\PROJECT_NGINE\\src\\shaders\\gaussian.vs",
        "C:\\Dev\\git\\PROJECT_NGINE\\PROJECT_NGINE\\src\\shaders\\gaussian.fs"
    );
    
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
            (void*)(offsetof(GaussianGPU, sh) + i * sizeof(glm::vec4)));
        glVertexAttribDivisor(3 + i, 1);
    }

    glBindVertexArray(0);

    // --- GL state for correct alpha blending ---
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied alpha
    glDisable(GL_DEPTH_TEST);                     // rely on CPU sort order

}

void GaussianRenderer::render(vector<Gaussian>& Gaussians, vector<int>& sorted_indices, int campos)
{
    // create shaders
    // bind vaos
    const int n = static_cast<int>(sorted_indices.size());

    // --- Build sorted GPU buffer ---
    m_gpu_buffer.resize(n);
    for (int i = 0; i < n; ++i) {
        m_gpu_buffer[i] = to_gpu(gaussians[sorted_indices[i]]);
    }

    // Upload to GPU
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 n * sizeof(GaussianGPU),
                 m_gpu_buffer.data(),
                 GL_DYNAMIC_DRAW);

    // --- Set uniforms ---
    glUseProgram(m_program);

    mat4x4 view = camera.view_matrix();
    mat4x4 proj = camera.projection_matrix();

    glUniformMatrix4fv(glGetUniformLocation(m_program, "u_view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(m_program, "u_proj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform2f(glGetUniformLocation(m_program, "u_focal"), camera.focal_x(), camera.focal_y());
    glUniform2f(glGetUniformLocation(m_program, "u_viewport"), (float)camera.width, (float)camera.height);

    // --- Draw ---
    glBindVertexArray(VAO);
    // 4 vertices per quad (TRIANGLE_STRIP), n instances (one per Gaussian)
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n);
    glBindVertexArray(0);

}


