#define STB_IMAGE_IMPLEMENTATION
#include "Application.h"
#include "Config.h"
#include "Scene.h"
#include "RenderPipeline.h"
#include "OBJLoader.h"
#include "ply_loader.h"
#include "ParticleRenderer.h"
#include "3DGS_renderer.h"
#include "shader.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "stb_image.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

namespace {
void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    void* user = glfwGetWindowUserPointer(window);
    if (user)
        static_cast<Application*>(user)->framebufferSizeCallback(width, height);
}

GLuint loadTexture2D(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "STB failed to load: " << path << " - " << stbi_failure_reason() << "\n";
        return 0;
    }
    GLenum format = (channels == 1) ? GL_RED : (channels == 3) ? GL_RGB : GL_RGBA;
    if (channels != 1 && channels != 3 && channels != 4) {
        stbi_image_free(data);
        return 0;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return tex;
}
}  // namespace

Application::Application(const Config& config, const AppArgs& args)
    : config_(config), args_(args) {}

Application::~Application() {
    if (!initialized_) {
        if (window_) {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
        return;
    }
    if (pipeline_) delete pipeline_;
    if (gaussianRenderer_) delete gaussianRenderer_;
    if (particleRenderer_) delete particleRenderer_;
    if (simulator_) delete simulator_;
    if (phongShader_) {
        phongShader_->deleteProgram();
        delete phongShader_;
    }
    scene_.destroy();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool Application::init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(config_.windowWidth, config_.windowHeight, "NGine", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, ::framebufferSizeCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        return false;
    }
    glViewport(0, 0, config_.windowWidth, config_.windowHeight);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    phongShader_ = new shader(config_.phongVsPath.c_str(), config_.phongFsPath.c_str());

    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
    bool ok = LoadOBJ_Indexed(config_.objPath, verts, indices);
    std::cout << "OBJ ok=" << ok << " verts=" << verts.size() << " indices=" << indices.size() << "\n";
    scene_.mesh.upload(verts, indices);

    scene_.textureId = loadTexture2D(config_.texturePath);
    if (!scene_.textureId) {
        std::cerr << "Failed to load texture: " << config_.texturePath << "\n";
        scene_.destroy();
        delete phongShader_;
        phongShader_ = nullptr;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        return false;
    }

    if (!args_.plyPath.empty()) {
        scene_.gaussians = load_ply(args_.plyPath);
        if (scene_.gaussians.empty())
            std::cerr << "No Gaussians loaded from " << args_.plyPath << "; 3DGS disabled.\n";
    }

    scene_.camera.fovDeg = config_.fovDeg;

    simulator_ = new SPHSimulator();
    particleRenderer_ = new ParticleRenderer();
    particleRenderer_->init(config_.particleVsPath, config_.particleFsPath);
    gaussianRenderer_ = new GaussianRenderer();
    gaussianRenderer_->init(config_.gaussianVsPath, config_.gaussianFsPath);
    pipeline_ = new RenderPipeline(phongShader_, gaussianRenderer_, particleRenderer_);

    glfwSwapInterval(1);
    initialized_ = true;
    return true;
}

void Application::run() {
    while (!glfwWindowShouldClose(window_)) {
        processInput(0.016f);
        simulator_->step();
        pipeline_->render(scene_, *simulator_,
            config_.windowWidth, config_.windowHeight,
            config_.zNear, config_.zFar);
        glfwSwapBuffers(window_);
        glfwPollEvents();
    }
}

void Application::processInput(float dt) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;

    const float moveSpeed = 0.03f;
    const float rotSpeed = 2.0f * (3.14159265f / 180.0f);

    if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
        scene_.camera.position.y += moveSpeed;
    if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
        scene_.camera.position.y -= moveSpeed;
    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
        scene_.camera.position.x += moveSpeed;
    if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
        scene_.camera.position.x -= moveSpeed;
    if (glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS)
        scene_.camera.yaw -= rotSpeed;
    if (glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS)
        scene_.camera.yaw += rotSpeed;

    vec3f forward = scene_.camera.getForward();
    vec3f delta = vector_mul(forward, moveSpeed);
    if (glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS)
        scene_.camera.position = vector_sub(scene_.camera.position, delta);
    if (glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS)
        scene_.camera.position = vector_add(scene_.camera.position, delta);
}

void Application::framebufferSizeCallback(int width, int height) {
    config_.windowWidth = width;
    config_.windowHeight = height;
    glViewport(0, 0, width, height);
}
