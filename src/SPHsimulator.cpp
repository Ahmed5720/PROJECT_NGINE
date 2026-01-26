#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "miniVM.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

// ============================================================================
// CONSTANTS AND CONFIGURATION
// ============================================================================

const int PARTICLE_COUNT = 5000;
const float SMOOTHING_RADIUS = 0.05f;
const float PARTICLE_MASS = 0.02f;
const float REST_DENSITY = 1000.0f;
const float PRESSURE_CONSTANT = 200.0f;
const float VISCOSITY_CONSTANT = 0.01f;
const float GRAVITY = -9.8f;
const float BOUNCE_DAMPING = 0.5f;
const float TIME_STEP = 0.001f;
const float M_PI = 3.14;
const vec3f BOX_MIN(-1.0f, -1.0f, -1.0f);
const vec3f BOX_MAX(1.0f, 1.0f, 1.0f);

// Hash grid parameters
const int HASH_TABLE_SIZE = 16384;  // Should be prime or power of 2

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Particle {
    vec3f position;
    float life;
    vec3f color;
    vec3f velocity;
    float _pad;
};

struct SPHData {
    float density;
    float pressure;
    vec2f _pad0;
    vec3f force;
    float _pad1;
};

struct SimParams {
    float dt;
    float bounce;
    uint32_t particleCount;
    float gravity;
    
    float smoothingRadius;
    float smoothingRadius2;
    float mass;
    float restDensity;
    
    float pressureConstant;
    float viscosityConstant;
    vec2f _pad0;
    
    vec3f boxMin;
    float poly6Constant;
    
    vec3f boxMax;
    float spikyConstant;
    
    vec3f gridDim;
    float cellSize;
    
    uint32_t tableSize;
    vec3f _pad1;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::string loadShaderSource(const std::string& filepath) {
    std::ifstream file(filepath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compileComputeShader(const std::string& source, const std::string& define) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    std::string fullSource = "#version 430 core\n#define " + define + "\n" + source;
    const char* src = fullSource.c_str();
    
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed (" << define << "):\n" << infoLog << std::endl;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);
    
    return program;
}

float poly6Constant(float h) {
    return 315.0f / (64.0f * M_PI * std::pow(h, 9));
}

float spikyConstant(float h) {
    return -45.0f / (M_PI * std::pow(h, 6));
}

vec3i calculateGridDimensions(vec3f boxMin, vec3f boxMax, float cellSize) {
    vec3f extent = vector_sub(boxMax, boxMin);
    return vec3i(
        std::ceil(extent.x / cellSize),
        std::ceil(extent.y / cellSize),
        std::ceil(extent.z / cellSize)
    );
}

// ============================================================================
// SPH SIMULATOR CLASS
// ============================================================================

class SPHSimulator {
public:
    SPHSimulator() {
        initializeBuffers();
        initializeShaders();
        initializeParticles();
    }
    
    ~SPHSimulator() {
        glDeleteBuffers(1, &particleBuffer);
        glDeleteBuffers(1, &sphDataBuffer);
        glDeleteBuffers(1, &hashBuffer);
        glDeleteBuffers(1, &indexBuffer);
        glDeleteBuffers(1, &cellStartBuffer);
        glDeleteBuffers(1, &cellEndBuffer);
        glDeleteBuffers(1, &paramsUBO);
        
        glDeleteProgram(computeHashProgram);
        glDeleteProgram(buildCellIndexProgram);
        glDeleteProgram(computeDensityProgram);
        glDeleteProgram(computeForcesProgram);
        glDeleteProgram(integrateProgram);
    }
    
    void step() {
        // Stage 1: Compute spatial hash for each particle
        glUseProgram(computeHashProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Stage 2: Sort particles by hash on CPU
        sortParticlesByHash();
        
        // Stage 3: Build cell start/end indices
        glUseProgram(buildCellIndexProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Stage 4: Compute density and pressure
        glUseProgram(computeDensityProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Stage 5: Compute forces
        glUseProgram(computeForcesProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Stage 6: Integrate positions and velocities
        glUseProgram(integrateProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    
    GLuint getParticleBuffer() const { return particleBuffer; }
    
private:
    GLuint particleBuffer;
    GLuint sphDataBuffer;
    GLuint hashBuffer;
    GLuint indexBuffer;
    GLuint cellStartBuffer;
    GLuint cellEndBuffer;
    GLuint paramsUBO;
    
    GLuint computeHashProgram;
    GLuint buildCellIndexProgram;
    GLuint computeDensityProgram;
    GLuint computeForcesProgram;
    GLuint integrateProgram;
    
    std::vector<uint32_t> hashData;
    std::vector<uint32_t> indexData;
    
    void initializeBuffers() {
        // Particle buffer
        glGenBuffers(1, &particleBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     PARTICLE_COUNT * sizeof(Particle), 
                     nullptr, GL_DYNAMIC_DRAW);
        
        // SPH data buffer
        glGenBuffers(1, &sphDataBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphDataBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     PARTICLE_COUNT * sizeof(SPHData), 
                     nullptr, GL_DYNAMIC_DRAW);
        
        // Hash buffer
        glGenBuffers(1, &hashBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, hashBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     PARTICLE_COUNT * sizeof(uint32_t), 
                     nullptr, GL_DYNAMIC_DRAW);
        
        // Index buffer
        glGenBuffers(1, &indexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     PARTICLE_COUNT * sizeof(uint32_t), 
                     nullptr, GL_DYNAMIC_DRAW);
        
        // Cell start buffer
        glGenBuffers(1, &cellStartBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellStartBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     HASH_TABLE_SIZE * sizeof(uint32_t), 
                     nullptr, GL_DYNAMIC_DRAW);
        
        // Cell end buffer
        glGenBuffers(1, &cellEndBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellEndBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     HASH_TABLE_SIZE * sizeof(uint32_t), 
                     nullptr, GL_DYNAMIC_DRAW);
        
        // Initialize cell start/end to 0
        std::vector<uint32_t> zeros(HASH_TABLE_SIZE, 0);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellStartBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       HASH_TABLE_SIZE * sizeof(uint32_t), zeros.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellEndBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       HASH_TABLE_SIZE * sizeof(uint32_t), zeros.data());
        
        // Simulation parameters UBO
        SimParams params;
        params.dt = TIME_STEP;
        params.bounce = BOUNCE_DAMPING;
        params.particleCount = PARTICLE_COUNT;
        params.gravity = GRAVITY;
        params.smoothingRadius = SMOOTHING_RADIUS;
        params.smoothingRadius2 = SMOOTHING_RADIUS * SMOOTHING_RADIUS;
        params.mass = PARTICLE_MASS;
        params.restDensity = REST_DENSITY;
        params.pressureConstant = PRESSURE_CONSTANT;
        params.viscosityConstant = VISCOSITY_CONSTANT;
        params.boxMin = BOX_MIN;
        params.boxMax = BOX_MAX;
        params.poly6Constant = poly6Constant(SMOOTHING_RADIUS);
        params.spikyConstant = spikyConstant(SMOOTHING_RADIUS);
        params.cellSize = SMOOTHING_RADIUS;
        params.gridDim = vec3f(calculateGridDimensions(BOX_MIN, BOX_MAX, SMOOTHING_RADIUS));
        params.tableSize = HASH_TABLE_SIZE;
        
        glGenBuffers(1, &paramsUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, paramsUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(SimParams), &params, GL_STATIC_DRAW);
        
        // Initialize CPU-side arrays for sorting
        hashData.resize(PARTICLE_COUNT);
        indexData.resize(PARTICLE_COUNT);
    }
    
    void initializeShaders() {
        // Load shader source (you would load from file in practice)
        std::string shaderSource = loadShaderSource("sph_compute.glsl");
        
        computeHashProgram = compileComputeShader(shaderSource, "COMPUTE_HASH");
        buildCellIndexProgram = compileComputeShader(shaderSource, "BUILD_CELL_INDEX");
        computeDensityProgram = compileComputeShader(shaderSource, "COMPUTE_DENSITY");
        computeForcesProgram = compileComputeShader(shaderSource, "COMPUTE_FORCES");
        integrateProgram = compileComputeShader(shaderSource, "INTEGRATE");
        
        // Bind buffers to all programs
        auto bindBuffers = [&](GLuint program) {
            glUseProgram(program);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sphDataBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, hashBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, indexBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cellStartBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, cellEndBuffer);
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, paramsUBO);
        };
        
        bindBuffers(computeHashProgram);
        bindBuffers(buildCellIndexProgram);
        bindBuffers(computeDensityProgram);
        bindBuffers(computeForcesProgram);
        bindBuffers(integrateProgram);
    }
    
    void initializeParticles() {
        std::vector<Particle> particles(PARTICLE_COUNT);
        
        // Initialize particles in a dam break configuration
        int particlesPerDim = std::cbrt(PARTICLE_COUNT);
        float spacing = SMOOTHING_RADIUS * 0.8f;
        
        int idx = 0;
        for (int x = 0; x < particlesPerDim && idx < PARTICLE_COUNT; x++) {
            for (int y = 0; y < particlesPerDim && idx < PARTICLE_COUNT; y++) {
                for (int z = 0; z < particlesPerDim && idx < PARTICLE_COUNT; z++) {
                    particles[idx].position = vec3f(
                        BOX_MIN.x + 0.1f + x * spacing,
                        BOX_MIN.y + 0.1f + y * spacing,
                        BOX_MIN.z + 0.1f + z * spacing
                    );
                    particles[idx].velocity = vec3f(0.0f,0.0f,0.0f);
                    particles[idx].color = vec3f(0.3f, 0.5f, 1.0f, 1.0f);
                    particles[idx].life = 1.0f;
                    idx++;
                }
            }
        }
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       PARTICLE_COUNT * sizeof(Particle), particles.data());
    }
    
    void sortParticlesByHash() {
        // Download hash and index data from GPU
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, hashBuffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                          PARTICLE_COUNT * sizeof(uint32_t), hashData.data());
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                          PARTICLE_COUNT * sizeof(uint32_t), indexData.data());
        
        // Create index pairs for sorting
        std::vector<std::pair<uint32_t, uint32_t>> hashIndexPairs(PARTICLE_COUNT);
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            hashIndexPairs[i] = {hashData[i], indexData[i]};
        }
        
        // Sort by hash value
        std::sort(hashIndexPairs.begin(), hashIndexPairs.end(),
                 [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // Extract sorted data
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            hashData[i] = hashIndexPairs[i].first;
            indexData[i] = hashIndexPairs[i].second;
        }
        
        // Upload sorted data back to GPU
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, hashBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       PARTICLE_COUNT * sizeof(uint32_t), hashData.data());
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       PARTICLE_COUNT * sizeof(uint32_t), indexData.data());
        
        // Clear cell start/end buffers
        std::vector<uint32_t> maxVals(HASH_TABLE_SIZE, PARTICLE_COUNT);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellStartBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       HASH_TABLE_SIZE * sizeof(uint32_t), maxVals.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellEndBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       HASH_TABLE_SIZE * sizeof(uint32_t), maxVals.data());
    }
};

// ============================================================================
// MAIN APPLICATION
// ============================================================================

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "SPH Fluid Simulation", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }
    
    // Create simulator
    SPHSimulator simulator;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Run simulation steps
        for (int i = 0; i < 5; i++) {  // Multiple substeps per frame
            simulator.step();
        }
        
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwTerminate();
    return 0;
}