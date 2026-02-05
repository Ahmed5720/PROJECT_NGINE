#pragma once
#include <glad/glad.h> 
#include <fstream>
#include <sstream>
#include <iostream>
using namespace std;
class shader
{
    public: unsigned int ID;

    shader(const char* vertexPath, const char* fragmentPath)
    {
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;

        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        try
        {
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);

            std::stringstream vShaderStream, fShaderStream;

            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            // close file handlers
            vShaderFile.close();
            fShaderFile.close();

            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();

        }
        catch(std::ifstream::failure& e){
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        }
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        unsigned int vertex, fragment;

        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        // fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");
        // shader Program
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);


        
    }

    shader(const char* computePath, const char* define, int c)
    {
        string source= loadShaderSource(computePath);
        ID = compileComputeShader(source, define);
    }

     // Utility uniform functions
    void setBool(const std::string &name, bool value) const
    {         
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value); 
    }
    
    void setInt(const std::string &name, int value) const
    { 
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value); 
    }
    
    void setFloat(const std::string &name, float value) const
    { 
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value); 
    }
    
    void setFloat3(const std::string &name, float x, float y, float z) const
    { 

        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc == -1)
            std::cout << "Warning: Uniform '" << name << "' not found!" << std::endl;
        glUniform3f(loc, x, y, z); 
  

    }

    void setMat4(const std::string &name, const float* m) const
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, m);
    }

    
    // Dispatch compute shader
    void dispatch(GLuint numGroupsX, GLuint numGroupsY = 1, GLuint numGroupsZ = 1) const
    {
        glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
    }

    //Memory barrier for compute shader synchronization
    static void memoryBarrier(GLbitfield barriers = GL_ALL_BARRIER_BITS)
    {
        glMemoryBarrier(barriers);
    }

    void use()
    {
        glUseProgram(ID);

        GLenum err = glGetError();
        if (err != GL_NO_ERROR)
            std::cout << "OpenGL error after glUseProgram: " << err << std::endl;
    }
    void deleteProgram()
    {
        glDeleteProgram(ID);
    }


    std::string loadShaderSource(const std::string& filepath) {
        std::ifstream file(filepath);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    GLuint compileComputeShader(const std::string& source, const std::string& define) {
        GLuint sh = glCreateShader(GL_COMPUTE_SHADER);

        std::string fullSource = "#version 430 core\n"
                     "#define " + define + "\n"
                     + source;
        const char* src = fullSource.c_str();

        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);

        GLint compiled = GL_FALSE;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint len = 0;
            glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            glGetShaderInfoLog(sh, len, &len, log.data());
            std::cerr << "Compute shader compile failed (" << define << "):\n" << log << "\n";
        }

        GLuint prog = glCreateProgram();
        glAttachShader(prog, sh);
        glLinkProgram(prog);
        glDeleteShader(sh);

        GLint linked = GL_FALSE;
        glGetProgramiv(prog, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint len = 0;
            glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            glGetProgramInfoLog(prog, len, &len, log.data());
            std::cerr << "Compute program link failed (" << define << "):\n" << log << "\n";
        }

        // This is the key: reject unusable programs early
        if (!compiled || !linked) {
            glDeleteProgram(prog);
            return 0;
        }

        return prog;
    }

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