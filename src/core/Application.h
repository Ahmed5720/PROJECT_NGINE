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

    std::shared_ptr<PhysX> simulator_;
    std::unique_ptr<shader> phongShader_;
    std::unique_ptr<shader> pbrShader_ ;
    std::unique_ptr<shader> wireFrameShader_ ;
    std::unique_ptr<shader> skyBoxShader_ ;
    std::unique_ptr<shader> shadowShader_ ;
    std::unique_ptr<shader> captureHdrShader_ ;
    std::unique_ptr<shader> prefilterShader_ ;
    std::unique_ptr<shader> brdfShader_ ;
    std::unique_ptr<shader> convolveShader_ ;
    std::unique_ptr<ParticleRenderer> particleRenderer_ ;
    //GaussianRenderer* gaussianRenderer_ ;
    std::unique_ptr<RenderPipeline> pipeline_ ;
    int projectileMeshIndex_ = -1;
};
