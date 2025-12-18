#pragma once
#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

class Shader {
public:
    GLuint ID = 0;

    Shader() = default;

    // Build from files
    Shader(const char* vertexPath, const char* fragmentPath) {
        loadFromFiles(vertexPath, fragmentPath);
    }

    // Build compute from file
    Shader(const char* computePath) {
        loadComputeFromFile(computePath);
    }

    ~Shader() {
        if (ID) glDeleteProgram(ID);
    }

    // Non-copyable (owning GL object)
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Movable
    Shader(Shader&& other) noexcept { *this = std::move(other); }
    Shader& operator=(Shader&& other) noexcept {
        if (this != &other) {
            if (ID) glDeleteProgram(ID);
            ID = other.ID;
            other.ID = 0;
        }
        return *this;
    }

    void use() const {
        glUseProgram(ID);
    }

    // Compile from source strings
    void compileFromSource(const char* vertexSrc, const char* fragmentSrc) {
        GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSrc, "VERTEX(src)");
        GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc, "FRAGMENT(src)");
        linkProgram({ vs, fs });
        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    void compileComputeFromSource(const char* computeSrc) {
        GLuint cs = compileStage(GL_COMPUTE_SHADER, computeSrc, "COMPUTE(src)");
        linkProgram({ cs });
        glDeleteShader(cs);
    }

    // Compile from files
    void loadFromFiles(const char* vertexPath, const char* fragmentPath) {
        std::string vCode = readFile(vertexPath);
        std::string fCode = readFile(fragmentPath);

        GLuint vs = compileStage(GL_VERTEX_SHADER, vCode.c_str(), vertexPath);
        GLuint fs = compileStage(GL_FRAGMENT_SHADER, fCode.c_str(), fragmentPath);
        linkProgram({ vs, fs });
        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    void loadComputeFromFile(const char* computePath) {
        std::string cCode = readFile(computePath);
        GLuint cs = compileStage(GL_COMPUTE_SHADER, cCode.c_str(), computePath);
        linkProgram({ cs });
        glDeleteShader(cs);
    }

    // Uniform helpers 
    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }

    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

private:
    static std::string readFile(const char* path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file) {

            std::cout << "tezak hamra";
            std::ostringstream oss;
            oss << "Failed to open shader file: " << path;
            throw std::runtime_error(oss.str());
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

    static GLuint compileStage(GLenum stage, const char* src, const char* label) {
        GLuint shader = glCreateShader(stage);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint logLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
            std::string log((size_t)logLen, '\0');
            glGetShaderInfoLog(shader, logLen, nullptr,
                log.empty() ? nullptr : &log[0]);

            std::ostringstream oss;
            oss << "Shader compile failed [" << label << "]\n" << log << "\n";
            glDeleteShader(shader);
            throw std::runtime_error(oss.str());
        }
        return shader;
    }

    void linkProgram(std::initializer_list<GLuint> shaders) {
        if (ID) glDeleteProgram(ID);

        ID = glCreateProgram();
        for (GLuint s : shaders) glAttachShader(ID, s);
        glLinkProgram(ID);
        for (GLuint s : shaders) glDetachShader(ID, s);

        GLint ok = 0;
        glGetProgramiv(ID, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint logLen = 0;
            glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &logLen);
            std::string log((size_t)logLen, '\0');
            glGetProgramInfoLog(ID, logLen, nullptr,
                log.empty() ? nullptr : &log[0]);

            std::ostringstream oss;
            oss << "Program link failed\n" << log << "\n";
            glDeleteProgram(ID);
            ID = 0;
            throw std::runtime_error(oss.str());
        }
    }
};
