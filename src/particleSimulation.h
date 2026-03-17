#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "miniVM.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include "shader.h"
// ============================================================================
// CONSTANTS AND CONFIGURATION (inline for single definition when header included in multiple TUs)
// ============================================================================

inline const int PARTICLE_COUNT = 1000;
inline float SMOOTHING_RADIUS = 0.5f;
inline float PARTICLE_MASS = 1.0f;
inline float REST_DENSITY = 1000.0f;
inline float PRESSURE_CONSTANT = 0.0f;
inline float VISCOSITY_CONSTANT = 0.0f;
inline float GRAVITY = 0.0f;
inline float BOUNCE_DAMPING = 0.5f;
inline float TIME_STEP = 0.01f;
inline float M_PI = 3.14f;
inline vec3f BOX_MIN(-1.0f, -1.0f, -1.0f);
inline vec3f BOX_MAX(1.0f, 1.0f, 1.0f);
inline constexpr float SPH_PI = 3.14159265358979f;
inline const int HASH_TABLE_SIZE = 16384;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct alignas(16) Particle {
    vec3f position;   // x,y,z,w (already 16 bytes)
    vec3f color;      // x,y,z,w (treat w as alpha)
    vec3f velocity;   // x,y,z,w
    float life;       // 4 bytes
    float _padLife[3];// pad to 16 bytes
};
static_assert(sizeof(Particle) == 64, "Particle must be 64 bytes");

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
    
    vec3i gridDim;
    float cellSize;
    
    uint32_t tableSize;
    vec3f _pad1;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================


inline float poly6Constant(float h) {
    return 315.0f / (64.0f * SPH_PI * std::pow(h, 9));
}

inline float spikyConstant(float h) {
    return -45.0f / (SPH_PI * std::pow(h, 6));
}

inline vec3i calculateGridDimensions(vec3f boxMin, vec3f boxMax, float cellSize) {
    vec3f extent = vector_sub(boxMax, boxMin);
    //cout << "grid dim is" << std::ceil(extent.x / cellSize) << std::ceil(extent.y / cellSize) << std::ceil(extent.z / cellSize);
    return vec3i(
        std::ceil(extent.x / cellSize),
        std::ceil(extent.y / cellSize),
        std::ceil(extent.z / cellSize)
    );
}

// ============================================================================
// SPH SIMULATOR CLASS
// ============================================================================
inline void checkComputeError(const char* stage) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error in " << stage << ": " << err << std::endl;
    }
}
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

    void reset_sim()
    {
        // Re-initialize particles
        initializeParticles();
        
        // Clear SPH data
        std::vector<SPHData> emptySPHData(PARTICLE_COUNT);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphDataBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       PARTICLE_COUNT * sizeof(SPHData), emptySPHData.data());
        
        // Clear hash and index buffers
        std::vector<uint32_t> zeros(PARTICLE_COUNT, 0);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, hashBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       PARTICLE_COUNT * sizeof(uint32_t), zeros.data());
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       PARTICLE_COUNT * sizeof(uint32_t), zeros.data());
        
        // Clear cell start/end buffers
        std::vector<uint32_t> maxVals(HASH_TABLE_SIZE, PARTICLE_COUNT);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellStartBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       HASH_TABLE_SIZE * sizeof(uint32_t), maxVals.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellEndBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                       HASH_TABLE_SIZE * sizeof(uint32_t), maxVals.data());
        
        // Reset CPU-side arrays
        hashData.assign(PARTICLE_COUNT, 0);
        indexData.assign(PARTICLE_COUNT, 0);
        
        std::cout << "Simulation reset!" << std::endl;
    }

    GLuint getParticleBuffer() const { return particleBuffer; }
    
    void step() {

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
        params.gridDim = calculateGridDimensions(BOX_MIN, BOX_MAX, SMOOTHING_RADIUS);
        params.tableSize = HASH_TABLE_SIZE;
     
        glBindBuffer(GL_UNIFORM_BUFFER, paramsUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(SimParams), &params, GL_STATIC_DRAW);


        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sphDataBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, hashBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, indexBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cellStartBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, cellEndBuffer);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, paramsUBO);

       // Stage 1: Compute spatial hash for each particle
        glUseProgram(computeHashProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        checkComputeError("COMPUTE_HASH");
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        //Stage 2: Sort particles by hash on CPU
        sortParticlesByHash();
        
        // Stage 3: Build cell start/end indices
        glUseProgram(buildCellIndexProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        checkComputeError("COMPUTE_HASH");
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Stage 4: Compute density and pressure
        glUseProgram(computeDensityProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        checkComputeError("COMPUTE_HASH");
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Stage 5: Compute forces
        glUseProgram(computeForcesProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        checkComputeError("COMPUTE_HASH");
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Stage 6: Integrate positions and velocities
        glUseProgram(integrateProgram);
        glDispatchCompute((PARTICLE_COUNT + 63) / 64, 1, 1);
        checkComputeError("COMPUTE_HASH");
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL error after dispatch: " << err << std::endl;
        }

    }
    
  
    
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
        
        glGenBuffers(1, &hashBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, hashBuffer);
        std::vector<uint32_t> zeroHashes(PARTICLE_COUNT, 0u);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                    PARTICLE_COUNT * sizeof(uint32_t),
                    zeroHashes.data(), GL_DYNAMIC_DRAW);
        
        // Index buffer
        std::vector<uint32_t> identityIndices(PARTICLE_COUNT);
        std::iota(identityIndices.begin(), identityIndices.end(), 0u);
        glGenBuffers(1, &indexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                    PARTICLE_COUNT * sizeof(uint32_t),
                    identityIndices.data(), GL_DYNAMIC_DRAW);
        
        std::vector<uint32_t> sentinel(HASH_TABLE_SIZE, PARTICLE_COUNT);
    
        glGenBuffers(1, &cellStartBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellStartBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                    HASH_TABLE_SIZE * sizeof(uint32_t),
                    sentinel.data(), GL_DYNAMIC_DRAW);

        glGenBuffers(1, &cellEndBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellEndBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                    HASH_TABLE_SIZE * sizeof(uint32_t),
                    sentinel.data(), GL_DYNAMIC_DRAW);

        // CPU-side mirror arrays — also init to identity
        hashData.resize(PARTICLE_COUNT, 0u);
        indexData.resize(PARTICLE_COUNT);
        std::iota(indexData.begin(), indexData.end(), 0u);
        
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
        params.gridDim = calculateGridDimensions(BOX_MIN, BOX_MAX, SMOOTHING_RADIUS);
        params.tableSize = HASH_TABLE_SIZE;
        
        glGenBuffers(1, &paramsUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, paramsUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(SimParams), &params, GL_STATIC_DRAW);
        
        // Initialize CPU-side arrays for sorting
        hashData.resize(PARTICLE_COUNT);
        indexData.resize(PARTICLE_COUNT);
    }
    
    void initializeShaders() {
        
        const char* shaderPath = "C:\\Dev\\git\\PROJECT_NGINE\\PROJECT_NGINE\\src\\shaders\\sph_compute.glsl";
        shader computeHash(shaderPath,"COMPUTE_HASH", 0);
        computeHashProgram = computeHash.ID;

        shader buildCellIndexShader(shaderPath, "BUILD_CELL_INDEX", 0);
        buildCellIndexProgram = buildCellIndexShader.ID;
        
        shader computeDensityShader(shaderPath, "COMPUTE_DENSITY", 0);
        computeDensityProgram = computeDensityShader.ID;

        shader computeForcesShader(shaderPath, "COMPUTE_FORCES", 0);
        computeForcesProgram = computeForcesShader.ID;

        shader integrateShader(shaderPath, "INTEGRATE" , 0);
        integrateProgram = integrateShader.ID;

        if (!computeHashProgram || !buildCellIndexProgram || !computeDensityProgram || !computeForcesProgram || !integrateProgram) {
            std::cerr << "One or more compute programs failed.\n";
         //   std::exit(1);
        }
        
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
    void drawDebugBox() {
        // Define the 8 corners of your box
        vec3f corners[8] = {
            vec3f(BOX_MIN.x, BOX_MIN.y, BOX_MIN.z),  // 0: near bottom left
            vec3f(BOX_MAX.x, BOX_MIN.y, BOX_MIN.z),  // 1: near bottom right
            vec3f(BOX_MAX.x, BOX_MAX.y, BOX_MIN.z),  // 2: near top right
            vec3f(BOX_MIN.x, BOX_MAX.y, BOX_MIN.z),  // 3: near top left
            vec3f(BOX_MIN.x, BOX_MIN.y, BOX_MAX.z),  // 4: far bottom left
            vec3f(BOX_MAX.x, BOX_MIN.y, BOX_MAX.z),  // 5: far bottom right
            vec3f(BOX_MAX.x, BOX_MAX.y, BOX_MAX.z),  // 6: far top right
            vec3f(BOX_MIN.x, BOX_MAX.y, BOX_MAX.z)   // 7: far top left
        };
        
        // Define the 12 edges (pairs of corner indices)
        int edges[12][2] = {
            {0,1}, {1,2}, {2,3}, {3,0},  // near face
            {4,5}, {5,6}, {6,7}, {7,4},  // far face
            {0,4}, {1,5}, {2,6}, {3,7}   // connecting edges
        };
        
        // Draw using old-style GL (if you're using legacy OpenGL)
        glColor3f(1.0f, 0.0f, 0.0f);  // Red box
        glBegin(GL_LINES);
        for (int i = 0; i < 12; i++) {
            glVertex3f(corners[edges[i][0]].x, corners[edges[i][0]].y, corners[edges[i][0]].z);
            glVertex3f(corners[edges[i][1]].x, corners[edges[i][1]].y, corners[edges[i][1]].z);
        }
        glEnd();
    }
    void initializeParticles() {
        std::vector<Particle> particles(PARTICLE_COUNT);
        
        // Initialize particles in a dam break configuration
        int particlesPerDim = std::cbrt(PARTICLE_COUNT);
        //float spacing = SMOOTHING_RADIUS * 0.3f;
        float spacing = 0.01f; 
        //drawDebugBox();
        vec3f spawnMin(-0.3f, 0.2f, -0.3f);   // elevated, centered
        vec3f spawnMax( 0.3f, 0.8f,  0.3f);     
        float spacing_x = (spawnMax.x - spawnMin.x) / (float)(particlesPerDim);
        float spacing_y = (spawnMax.y - spawnMin.y) / (float)(particlesPerDim);
        float spacing_z = (spawnMax.z - spawnMin.z) / (float)(particlesPerDim);
        int idx = 0;
        for (int x = 0; x < particlesPerDim && idx < PARTICLE_COUNT; x++) {
            for (int y = 0; y < particlesPerDim && idx < PARTICLE_COUNT; y++) {
                for (int z = 0; z < particlesPerDim && idx < PARTICLE_COUNT; z++) {
                    

                    particles[idx].position.x = spawnMin.x + x * spacing_x;
                    particles[idx].position.y = spawnMin.y + y * spacing_y;
                    particles[idx].position.z = spawnMin.z + z * spacing_z;
                    particles[idx].velocity = vec3f(0.0f,0.0f,0.0f,0.0f);
                    particles[idx].color = vec3f(1.0f, 0.0f, 0.0f, 1.0f);
                    particles[idx].life = 1.0f;
                    particles[idx]._padLife[0] = particles[idx]._padLife[1] = particles[idx]._padLife[2] = 0.0f;
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

