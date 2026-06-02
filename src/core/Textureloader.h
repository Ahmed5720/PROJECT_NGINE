#pragma once
#include "Material.h"
#include <string>
#include <iostream>
#include <stb_image.h>
#include <glad/glad.h>

// TextureLoader
//   Stateless utility for loading image files into GL textures.
//   Returns a TextureHandle (move-only RAII wrapper around GLuint).
//
namespace TextureLoader {

inline TextureHandle loadFromFile(const std::string& path, bool logOnFailure) {
    if (path.empty()) return TextureHandle{};

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        if (logOnFailure)
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
            if (logOnFailure)
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

// Load an image from disk and upload it to the GPU.
inline TextureHandle load(const std::string& path) {
    return loadFromFile(path, true);
}

// Resolve a texture filename relative to the directory of an OBJ/MTL file.
inline std::string resolveRelative(const std::string& objPath,
                                   const std::string& textureName) {
    if (textureName.empty()) return {};
    size_t slash = objPath.find_last_of("/\\");
    std::string dir = (slash != std::string::npos) ? objPath.substr(0, slash + 1) : "";
    return dir + textureName;
}

// Load map_Kd from MTL: try asset texture dir, then beside OBJ/MTL, then obj/textures/.
inline TextureHandle loadMapKd(const std::string& objPath,
                               const std::string& textureDir,
                               const std::string& mapKd,
                               std::string* resolvedPath = nullptr) {
    if (mapKd.empty()) return {};

    const std::string besideObj = resolveRelative(objPath, mapKd);
    const std::string besideTextures = resolveRelative(objPath, "textures/" + mapKd);
    const std::string inTextureDir =
        textureDir.empty() ? std::string() : (textureDir.back() == '/' || textureDir.back() == '\\'
            ? textureDir + mapKd
            : textureDir + "/" + mapKd);

    const char* candidates[] = { inTextureDir.c_str(), besideObj.c_str(), besideTextures.c_str() };

    for (const char* path : candidates) {
        if (!path || !path[0]) continue;
        TextureHandle handle = loadFromFile(path, false);
        if (handle.valid()) {
            if (resolvedPath) *resolvedPath = path;
            std::cout << "[TextureLoader] Loaded '" << mapKd << "' from '" << path << "'\n";
            return handle;
        }
    }

    std::cerr << "[TextureLoader] Could not find '" << mapKd << "'\n"
              << "  texture dir: " << (inTextureDir.empty() ? "(none)" : inTextureDir) << "\n"
              << "  OBJ folder:  " << besideObj << "\n"
              << "  OBJ/textures: " << besideTextures << "\n";
    return {};
}

} 