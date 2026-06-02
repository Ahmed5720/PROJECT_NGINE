#define STB_IMAGE_IMPLEMENTATION
#include "Application.h"
#include "Config.h"
#include "Scene.h"
#include "RenderPipeline.h"
//#include "OBJLoader.h"
#include "ply_loader.h"
#include "ParticleRenderer.h"
#include "3DGS_renderer.h"
#include "shader.h"
#include "TextureLoader.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector> 
#include "OBJ_Loader.h"

namespace {
void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    void* user = glfwGetWindowUserPointer(window);
    if (user)
        static_cast<Application*>(user)->framebufferSizeCallback(width, height);
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

    // Load OBJ: each OBJ sub-mesh becomes one SceneNode with its own
    // Material.  The MeshGPU (GPU buffers) lives in scene_.meshes and
    // is referenced by index from the node.
    objl::Loader OBJLoader;
    bool ok = OBJLoader.LoadFile(config_.objPath);
    if (ok) {
        for (const objl::Mesh& mesh : OBJLoader.LoadedMeshes) {
            // Build vertex buffer
            std::vector<Vertex> vertices;
            vertices.reserve(mesh.Vertices.size());
            for (const objl::Vertex& v : mesh.Vertices) {
                vertices.push_back({
                    v.Position.X,  v.Position.Y,  v.Position.Z,
                    v.Normal.X,    v.Normal.Y,    v.Normal.Z,
                    v.TextureCoordinate.X,
                    1.0f - v.TextureCoordinate.Y   // flip V (OBJ origin is bottom-left)
                });
            }
 
            // Upload to GPU 
            MeshGPU gpuMesh;
            gpuMesh.upload(vertices, mesh.Indices);
            int meshIndex = static_cast<int>(scene_.meshes.size());
            scene_.meshes.push_back(std::move(gpuMesh));
 
            // Build Material from the MTL entry 
            Material mat;
            mat.name = mesh.MeshMaterial.name;
            mat.type = Material::Type::Phong;
 
            // Diffuse colour from MTL Kd (default white if not set)
            mat.diffuseColor[0] = mesh.MeshMaterial.Kd.X;
            mat.diffuseColor[1] = mesh.MeshMaterial.Kd.Y;
            mat.diffuseColor[2] = mesh.MeshMaterial.Kd.Z;
 
            // Specular / shininess from MTL Ns and Ks magnitude
            mat.shininess = (mesh.MeshMaterial.Ns > 0.0f) ? mesh.MeshMaterial.Ns : 32.0f;
            //float ksAvg = (mesh.MeshMaterial.Ks.X + mesh.MeshMaterial.Ks.Y + mesh.MeshMaterial.Ks.Z) / 3.0f;
            //mat.specularStrength = (ksAvg > 0.0f) ? ksAvg : 0.5f;
 
            // Diffuse texture (map_Kd)
            if (!mesh.MeshMaterial.map_Kd.empty()) {
                // Prefer textures beside the OBJ; fall back to src/textures/
                std::string texPath = TextureLoader::resolveRelative(config_.objPath, mesh.MeshMaterial.map_Kd);
                mat.diffuseMap = TextureLoader::load(texPath);
                if (!mat.diffuseMap.valid()) {
                    texPath = config_.texturePath + "/" + mesh.MeshMaterial.map_Kd;
                    mat.diffuseMap = TextureLoader::load(texPath);
                }
            }
 
            // Create SceneNode 
            SceneNode& node = scene_.addNode(mesh.MeshName, meshIndex, std::move(mat));
 
            std::cout << "[Application] Loaded mesh '" << node.name << "'"
                      << "  verts="   << mesh.Vertices.size()
                      << "  indices=" << mesh.Indices.size()
                      << "  material='" << node.material.name << "'"
                      << "  texture=" << (node.material.diffuseMap.valid() ? "yes" : "no")
                      << "\n";
        }
    }
    else {
    std::cerr << "[Application] Failed to load OBJ: " << config_.objPath << "\n";
    }

    scene_.camera.fovDeg = config_.fovDeg;

    // simulator_ = new SPHSimulator();
    particleRenderer_ = new ParticleRenderer();
    particleRenderer_->init(config_.particleVsPath, config_.particleFsPath);
    pipeline_ = new RenderPipeline(phongShader_, particleRenderer_);

    glfwSwapInterval(1);
    initialized_ = true;
    return true;
}

void Application::run() {
    while (!glfwWindowShouldClose(window_)) {
        processInput(0.016f);
        //simulator_->step();
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
