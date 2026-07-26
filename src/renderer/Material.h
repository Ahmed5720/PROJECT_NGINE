#pragma once
#include <string>
#include <glad/glad.h>

struct TextureHandle {
    GLuint id = 0;
 
    TextureHandle() = default;
    explicit TextureHandle(GLuint id) : id(id) {}
 
    // Non-copyable, movable
    TextureHandle(const TextureHandle&)            = delete;
    TextureHandle& operator=(const TextureHandle&) = delete;
 
    TextureHandle(TextureHandle&& o) noexcept : id(o.id) { o.id = 0; }
    TextureHandle& operator=(TextureHandle&& o) noexcept {
        if (this != &o) { destroy(); id = o.id; o.id = 0; }
        return *this;
    }
 
    bool valid() const { return id != 0; }
 
    void bind(GLenum unit = GL_TEXTURE0) const {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, id);
    }
 
    void destroy() {
        if (id) { glDeleteTextures(1, &id); id = 0; }
    }
 
    ~TextureHandle() { destroy(); }
};


struct Material {
    enum class Type { Phong, PBR };
 
    Type type = Type::Phong;
 
    //  Phong 
    float diffuseColor[3]  = {1.0f, 1.0f, 1.0f};
    TextureHandle diffuseMap;          // optional
    TextureHandle normalMap;
    TextureHandle roughnessMap;
    TextureHandle aoMap;
    TextureHandle metallicMap;

    TextureHandle specularMap;  // tbh i should remove this shit, too much work to be able to switch between the two models
    float shininess = 32.0f;    // Phong exponent deprecated

    float roughness = 0.5f;
    float metallic = 0;
    float alpha = 1.0;
    bool emissive = false;
    std::string name;

    Material()                           = default;
    Material(const Material&)            = delete;
   // Material& operator=(const Material&) = delete;
    Material(Material&&)                 = default;
    Material& operator=(Material&&)      = default;
};