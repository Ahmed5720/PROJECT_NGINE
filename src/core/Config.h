#pragma once
#include <string>

struct AppArgs {
    std::string plyPath;
};

inline AppArgs parseArgs(int argc, char** argv) {
    AppArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ply" && i + 1 < argc)
            args.plyPath = argv[++i];
    }
    return args;
}

struct Config {
    int windowWidth  = 1200;
    int windowHeight = 1200;
    float fovDeg     = 50.0f;
    float zNear      = 0.1f;
    float zFar       = 1000.0f;

    std::string basePath;
    std::string objPath;
    std::string texturePath;
    std::string shaderDir;
    std::string phongVsPath;
    std::string phongFsPath;
    std::string wireVsPath;
    std::string wireFsPath;
    std::string particleVsPath;
    std::string particleFsPath;
    std::string gaussianVsPath;
    std::string gaussianFsPath;
    std::string skyboxVsPath;
    std::string skyboxFsPath;
    std::string sphComputePath;
    std::string shadowVsPath;
    std::string shadowFsPath;
    std::string pbrVsPath;
    std::string pbrFsPath;
    std::string captureHdrShaderVsPath;
    std::string captureHdrShaderFsPath;
    std::string hdr;
    std::string prefilterShaderVsPath;
    std::string prefilterShaderFsPath;
    std::string brdfShaderVsPath;
    std::string brdfShaderFsPath;
    std::string convolveShaderVsPath;
    std::string convolveShaderFsPath;

    void resolvePaths() {
        if (basePath.empty())
            basePath = ".";
        auto slash = [](const std::string& a, const std::string& b) {
            if (a.empty()) return b;
            if (a.back() == '/' || a.back() == '\\') return a + b;
            return a + "/" + b;
        };
        // basepath = src
        shaderDir      = slash(basePath, "shaders");
        phongVsPath    = slash(shaderDir, "shader.vs");
        phongFsPath    = slash(shaderDir, "shader.fs");
        pbrVsPath    = slash(shaderDir, "pbr.vs");
        pbrFsPath    = slash(shaderDir, "pbr.fs");
        wireVsPath     = slash(shaderDir, "wireFrame.vs");
        wireFsPath     = slash(shaderDir, "wireFrame.fs");
        particleVsPath = slash(shaderDir, "particle.vs");
        particleFsPath = slash(shaderDir, "particle.fs");
        skyboxVsPath   = slash(shaderDir, "skybox.vs");
        skyboxFsPath   = slash(shaderDir, "skybox.fs");
        shadowVsPath   = slash(shaderDir, "shadow.vs");
        shadowFsPath   = slash(shaderDir, "shadow.fs");
        sphComputePath = slash(shaderDir, "sph_compute.glsl");
        objPath        = slash(basePath, "..");
        objPath        = slash(objPath, "wineNew.obj");
        texturePath    = slash(basePath, "textures");
        hdr            = slash(texturePath, "cowboy.hdr");
        captureHdrShaderVsPath = slash(shaderDir, "hdrCapture.vs");
        captureHdrShaderFsPath = slash(shaderDir, "hdrCapture.fs");
        prefilterShaderVsPath = slash(shaderDir, "prefilter.vs");
        prefilterShaderFsPath = slash(shaderDir, "prefilter.fs");
        brdfShaderVsPath = slash(shaderDir, "brdf.vs");
        brdfShaderFsPath = slash(shaderDir, "brdf.fs");
        convolveShaderVsPath = slash(shaderDir, "convolve.vs");
        convolveShaderFsPath = slash(shaderDir, "convolve.fs");
    }
};
