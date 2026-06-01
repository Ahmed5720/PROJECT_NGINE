#pragma once
#include "Material.h"
#include <string>
#include <iostream>
#include <stb_image.h>
#include <glad/glad.h>

// ---------------------------------------------------------------------------
// TextureLoader
//   Stateless utility for loading image files into GL textures.
//   Returns a TextureHandle (move-only RAII wrapper around GLuint).
//
//   Previously this logic lived as a file-scoped lambda in Application.cpp.
//   Extracting it here means Application and future loaders can share it
//   without duplicating the stb_image boilerplate.
// ---------------------------------------------------------------------------
namespace TextureLoader {

// Load an image from disk and upload it to the GPU.
// Returns an invalid TextureHandle (id == 0) on failure.
inline TextureHandle load(const std::string& path) {
    if (path.empty()) return TextureHandle{};

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "[TextureLoader] Failed to load '" << path
                  << "': " << stbi_failure_reason() << "\n";
        return TextureHandle{};
    }

    GLenum format;
    switch (channels) {
        case 1:  format = GL_RED;  break;
        case 3:  format = GL_RGB;  break;
        case 4:  format = GL_RGBA; break;
        default:
            std::cerr << "[TextureLoader] Unsupported channel count " << channels
                      << " for '" << path << "'\n";
            stbi_image_free(data);
            return TextureHandle{};
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Sensible defaults — caller can override after getting the handle.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return TextureHandle{id};
}

// Resolve a texture filename relative to the directory of an OBJ file.
// e.g. resolveRelative("/assets/models/car.obj", "car_body.png")
//      -> "/assets/models/car_body.png"
inline std::string resolveRelative(const std::string& objPath,
                                   const std::string& textureName) {
    if (textureName.empty()) return {};
    size_t slash = objPath.find_last_of("/\\");
    std::string dir = (slash != std::string::npos) ? objPath.substr(0, slash + 1) : "";
    return dir + textureName;
}

} 