#pragma once
#include <glad/glad.h>
#include <vector>
#include <string>
#include "shader.h"
#include "geometry.h"

class ParticleRenderer {
public:
    ParticleRenderer();
    ~ParticleRenderer();
    
    void init();
    void init(const std::string& vertexPath, const std::string& fragmentPath);
    void render(GLuint particleBuffer, int particleCount, const mat4x4& view, const mat4x4& projection);
    
private:
    GLuint VAO;
    GLuint VBO;
    shader* particleShader;
};


ParticleRenderer::ParticleRenderer() : particleShader(nullptr), VAO(0), VBO(0) {
}

ParticleRenderer::~ParticleRenderer() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    delete particleShader;
}

void ParticleRenderer::init() {
    init("shaders/particle.vs", "shaders/particle.fs");
}

void ParticleRenderer::init(const std::string& vertexPath, const std::string& fragmentPath) {
    particleShader = new shader(vertexPath.c_str(), fragmentPath.c_str());
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glEnable(GL_PROGRAM_POINT_SIZE);
}

void ParticleRenderer::render(GLuint particleBuffer, int particleCount, 
                              const mat4x4& view, const mat4x4& projection) {
    if (!particleShader || particleCount <= 0)
        return;
    
    // Enable blending for transparent particles
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Disable depth writing for transparency
    
    particleShader->use();
    
    // Set uniforms
    float viewMat[16] = {
        view.m[0][0], view.m[0][1], view.m[0][2], view.m[0][3],
        view.m[1][0], view.m[1][1], view.m[1][2], view.m[1][3],
        view.m[2][0], view.m[2][1], view.m[2][2], view.m[2][3],
        view.m[3][0], view.m[3][1], view.m[3][2], view.m[3][3]
    };
    
    float projMat[16] = {
        projection.m[0][0], projection.m[0][1], projection.m[0][2], projection.m[0][3],
        projection.m[1][0], projection.m[1][1], projection.m[1][2], projection.m[1][3],
        projection.m[2][0], projection.m[2][1], projection.m[2][2], projection.m[2][3],
        projection.m[3][0], projection.m[3][1], projection.m[3][2], projection.m[3][3]
    };
    
    particleShader->use();
    particleShader->setMat4("view", viewMat);
    particleShader->setMat4("projection", projMat);
    particleShader->setFloat("pointSize", 5.0f); 
    

    // Bind SSBO at binding = 0 to match shader
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);

    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, particleCount);
    glBindVertexArray(0);

    // Unbind SSBO
    //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    
    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}