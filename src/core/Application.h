#pragma once
#include "Config.h"
#include "Scene.h"
#include "Camera.h"
#include <GLFW/glfw3.h>

class shader;
class SPHSimulator;
class GaussianRenderer;
class ParticleRenderer;
class RenderPipeline;

class Application {
public:
    Application(const Config& config, const AppArgs& args);
    ~Application();

    bool init();
    void run();

    void framebufferSizeCallback(int width, int height);

private:
    void processInput(float dt);

    Config config_;
    AppArgs args_;
    GLFWwindow* window_ = nullptr;
    bool initialized_ = false;

    Scene scene_;
    PhysX* simulator_ = nullptr;
    shader* phongShader_ = nullptr;
    shader* wireFrameShader_ = nullptr;
    ParticleRenderer* particleRenderer_ = nullptr;
    GaussianRenderer* gaussianRenderer_ = nullptr;
    RenderPipeline* pipeline_ = nullptr;
};
