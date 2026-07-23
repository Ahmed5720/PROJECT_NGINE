#define STB_IMAGE_IMPLEMENTATION
#include "Application.h"
#include "Config.h"
#include "Scene.h"
#include "RenderPipeline.h"
#include "EditorUI.h"
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
#include <thread>

namespace {

int uploadUnitCubeMesh(Scene& scene) {
    const float h = 0.5f;
    const vec3f positions[8] = {
        {-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
        {-h, -h,  h}, {h, -h,  h}, {h, h,  h}, {-h, h,  h}
    };
    const uint32_t faceIndices[6][6] = {
        {0, 1, 2, 0, 2, 3},
        {4, 6, 5, 4, 7, 6},
        {0, 4, 5, 0, 5, 1},
        {2, 6, 7, 2, 7, 3},
        {0, 3, 7, 0, 7, 4},
        {1, 5, 6, 1, 6, 2}
    };
    const vec3f faceNormals[6] = {
        {0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}, {-1, 0, 0}, {1, 0, 0}
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(36);
    indices.reserve(36);

    for (int face = 0; face < 6; ++face) {
        for (int tri = 0; tri < 6; ++tri) {
            const vec3f& p = positions[faceIndices[face][tri]];
            vertices.push_back({
                p.X, p.Y, p.Z,
                faceNormals[face].X, faceNormals[face].Y, faceNormals[face].Z,
                0.0f, 0.0f
            });
            indices.push_back(static_cast<uint32_t>(vertices.size() - 1));
        }
    }

    MeshGPU gpuMesh;
    gpuMesh.upload(vertices, indices);
    const int meshIndex = static_cast<int>(scene.meshes.size());
    scene.meshes.push_back(std::move(gpuMesh));
    return meshIndex;
}

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
    if (particleRenderer_) delete particleRenderer_;
    if (simulator_) delete simulator_;
    if (phongShader_) {
        phongShader_->deleteProgram();
        delete phongShader_;
    }
    if (wireFrameShader_)
    {
        wireFrameShader_->deleteProgram();
        delete wireFrameShader_;
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
    EditorUI::applyDarkTheme();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    phongShader_ = new shader(config_.phongVsPath.c_str(), config_.phongFsPath.c_str());
    wireFrameShader_ = new shader(config_.wireVsPath.c_str(), config_.wireFsPath.c_str());
    skyBoxShader_ = new shader(config_.skyboxVsPath.c_str(), config_.skyboxFsPath.c_str());

    // Load OBJ: each OBJ sub-mesh becomes one SceneNode with its own
    // Material.  The MeshGPU (GPU buffers) lives in scene_.meshes and
    // is referenced by index from the node.
    objl::Loader OBJLoader;
    bool ok = OBJLoader.LoadFile(config_.objPath);
    if (ok) {
        const float U = 1000.0f;
        const float D = -1000.0f;
        for (const objl::Mesh& mesh : OBJLoader.LoadedMeshes) {
            vec3f min = {U, U, U};
            vec3f max = {D, D, D};

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
                min.X = std::min(min.X, v.Position.X);
                min.Y = std::min(min.Y, v.Position.Y);
                min.Z = std::min(min.Z, v.Position.Z);
                max.X = std::max(max.X, v.Position.X);
                max.Y = std::max(max.Y, v.Position.Y);
                max.Z = std::max(max.Z, v.Position.Z);
            }
            // Upload to GPU 
            MeshGPU gpuMesh;
            gpuMesh.upload(vertices, mesh.Indices);
            int meshIndex = static_cast<int>(scene_.meshes.size());
            scene_.meshes.push_back(std::move(gpuMesh));

            const int rbIndex = static_cast<int>(scene_.rbs.size());
            RigidBody rb;
            rb.localMin = min;
            rb.localMax = max;
            scene_.rbs.push_back(rb);
 
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
                std::string resolved;
                mat.diffuseMap = TextureLoader::loadMapKd(
                    config_.objPath, config_.texturePath, mesh.MeshMaterial.map_Kd, &resolved);
                mat.specularMap = TextureLoader::loadMapKd(
                    config_.objPath, config_.texturePath, mesh.MeshMaterial.map_Ks, &resolved);
            }
 
            // Create SceneNode 
            SceneNode& node = scene_.addNode(mesh.MeshName, meshIndex, std::move(mat));
            node.rbIndex = rbIndex;

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

    // load cubemap textures
    vector<std::string> faces
    {
        "right.jpg",
        "left.jpg",
        "top.jpg",
        "bottom.jpg",
        "front.jpg",
        "back.jpg"
    };
    unsigned int cubeMapTexture = TextureLoader::loadCubemap(config_.texturePath, faces);
    scene_.cubeMapTexture = cubeMapTexture;
    scene_.camera.fovDeg = config_.fovDeg;
    projectileMeshIndex_ = uploadUnitCubeMesh(scene_);

    simulator_ = new PhysX(scene_);
    particleRenderer_ = new ParticleRenderer();
    particleRenderer_->init(config_.particleVsPath, config_.particleFsPath);
    pipeline_ = new RenderPipeline(phongShader_, wireFrameShader_, particleRenderer_, skyBoxShader_);

    glfwSwapInterval(1);
    initialized_ = true;
    return true;
}

void Application::run() {
    while (!glfwWindowShouldClose(window_)) {
        processInput(0.016f);
        pipeline_->render(scene_,config_.windowWidth, config_.windowHeight, config_.zNear, config_.zFar);
        // TO DO measure timing to see if this is any useful..
        std::thread physics_t(&PhysX::step, simulator_, 0.01, std::ref(scene_));
        physics_t.join();
        simulator_->updateTransforms(scene_);
            
        if (pipeline_->takeShootRequest() && simulator_ && projectileMeshIndex_ >= 0) {
            const vec3f forward = scene_.camera.getForward();
            simulator_->shootProjectile(scene_, projectileMeshIndex_, scene_.camera.position,
                                        forward, 25.0f, 0.25f);
        }

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
