#ifndef SHADER_C_H
#define SHADER_C_H

#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class ComputeShader
{  // this shader uses a texture instead of a SSBO 
public:
    unsigned int ID;
    unsigned int out_tex;
    // constructor generates the shader on the fly
    // ------------------------------------------------------------------------
    ComputeShader(const char* computePath, glm::uvec2 size)
    {
        work_size = size;
        // 1. retrieve the vertex/fragment source code from filePath
 
        std::string computeCode;
        std::ifstream cShaderFile;

        cShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try
        {
            cShaderFile.open(computePath);
            std::stringstream cShaderStream;
            // read file's buffer contents into streams
            cShaderStream << cShaderFile.rdbuf();
            // close file handlers
            cShaderFile.close();
            // convert stream into string

            computeCode = cShaderStream.str();
        }
        catch (std::ifstream::failure& e)
        {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        }
        const char* cShaderCode = computeCode.c_str();
        // 2. compile shaders
        unsigned int compute;
        // compute shader
        compute = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(compute, 1, &cShaderCode, NULL);
        glCompileShader(compute);
        checkCompileErrors(compute, "COMPUTE");

        // shader Program
        ID = glCreateProgram();
        glAttachShader(ID, compute);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");
        glDeleteShader(compute);


        // create input/output textures
        glGenTextures(1, &out_tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, out_tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // create empty texture
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size.x, size.y, 0, GL_RED, GL_FLOAT, NULL);
        glBindImageTexture(0, out_tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
    }


    ~ComputeShader()
    {
        glDeleteProgram(ID);
    }
    // activate the shader
    // ------------------------------------------------------------------------
    void use()
    {
        glUseProgram(ID);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, out_tex);
    }

    void dispatch() {
        // just keep it simple, 2d work group
        glDispatchCompute(work_size.x, work_size.y, 1);
    }


    void wait()
    {
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }


    void set_values(float* values) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, work_size.x, work_size.y, 0, GL_RED, GL_FLOAT, values);
    }

    std::vector<float> get_values() {
        unsigned int collection_size = work_size.x * work_size.y;
        std::vector<float> compute_data(collection_size);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, compute_data.data());

        return compute_data;
    }
private:

    private:
        glm::uvec2 work_size;
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    void checkCompileErrors(unsigned int shader, std::string type)
    {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};
#endif