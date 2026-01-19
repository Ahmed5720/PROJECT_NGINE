#version 430 core

// ============================================================================
// SHADER CONFIGURATION
// ============================================================================
// This file contains multiple compute shader stages:
// 1. COMPUTE_HASH - Calculate spatial hash for each particle
// 2. BUILD_CELL_INDEX - Build cell start/end lookup tables
// 3. COMPUTE_DENSITY - Calculate density and pressure
// 4. COMPUTE_FORCES - Calculate pressure and viscosity forces
// 5. INTEGRATE - Update particle positions and velocities
// ============================================================================

// Define which shader stage to compile
#ifdef COMPUTE_HASH
layout(local_size_x = 64) in;
#elif defined(BUILD_CELL_INDEX)
layout(local_size_x = 64) in;
#elif defined(COMPUTE_DENSITY)
layout(local_size_x = 64) in;
#elif defined(COMPUTE_FORCES)
layout(local_size_x = 64) in;
#elif defined(INTEGRATE)
layout(local_size_x = 64) in;
#endif

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Particle {
    vec3 position;
    float life;
    vec4 color;
    vec3 velocity;
    float _pad;
};

struct SPHData {
    float density;
    float pressure;
    vec2 _pad0;
    vec3 force;
    float _pad1;
};

struct SimParams {
    float dt;
    float bounce;
    uint particleCount;
    float gravity;
    
    float smoothingRadius;
    float smoothingRadius2;
    float mass;
    float restDensity;
    
    float pressureConstant;
    float viscosityConstant;
    vec2 _pad0;
    
    vec3 boxMin;
    float poly6Constant;
    
    vec3 boxMax;
    float spikyConstant;
    
    vec3 gridDim;      // Grid dimensions
    float cellSize;     // Cell size (= smoothing radius)
    
    uint tableSize;     // Hash table size
    uvec3 _pad1;
};

// ============================================================================
// BUFFER BINDINGS
// ============================================================================

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

layout(std430, binding = 1) buffer SPHDataBuffer {
    SPHData sphData[];
};

layout(std430, binding = 2) buffer HashBuffer {
    uint particleHashes[];
};

layout(std430, binding = 3) buffer IndexBuffer {
    uint particleIndices[];
};

layout(std430, binding = 4) buffer CellStartBuffer {
    uint cellStart[];
};

layout(std430, binding = 5) buffer CellEndBuffer {
    uint cellEnd[];
};

layout(std140, binding = 0) uniform SimParamsUBO {
    SimParams params;
};

// ============================================================================
// HASH GRID FUNCTIONS
// ============================================================================

ivec3 getGridCell(vec3 pos) {
    vec3 cellPos = (pos - params.boxMin) / params.cellSize;
    return ivec3(floor(cellPos));
}

uint hashCell(ivec3 cell) {
    // Spatial hash function with large primes
    const uint p1 = 73856093u;
    const uint p2 = 19349663u;
    const uint p3 = 83492791u;
    
    uint hash = (uint(cell.x) * p1) ^ (uint(cell.y) * p2) ^ (uint(cell.z) * p3);
    return hash % params.tableSize;
}

uint hashPosition(vec3 pos) {
    ivec3 cell = getGridCell(pos);
    return hashCell(cell);
}

// ============================================================================
// SPH KERNEL FUNCTIONS
// ============================================================================

float poly6Kernel(float r2) {
    float h2 = params.smoothingRadius2;
    if (r2 >= h2) {
        return 0.0;
    }
    float diff = h2 - r2;
    return params.poly6Constant * diff * diff * diff;
}

vec3 spikyGradient(float r, vec3 rNorm) {
    float h = params.smoothingRadius;
    if (r >= h || r <= 0.0) {
        return vec3(0.0);
    }
    float diff = h - r;
    return params.spikyConstant * diff * diff * rNorm;
}

float viscosityKernel(float r) {
    float h = params.smoothingRadius;
    float h2 = params.smoothingRadius2;
    float h3 = h2 * h;
    
    if (r >= h || r <= 0.0) {
        return 0.0;
    }
    
    return -(r * r * r) / (2.0 * h3) + (r * r) / h2 + h / (2.0 * r) - 1.0;
}

// ============================================================================
// STAGE 1: COMPUTE SPATIAL HASH
// ============================================================================

#ifdef COMPUTE_HASH

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= params.particleCount) return;
    
    vec3 pos = particles[i].position;
    particleHashes[i] = hashPosition(pos);
    particleIndices[i] = i;  // Will be sorted alongside hashes on CPU
}

#endif

// ============================================================================
// STAGE 2: BUILD CELL INDEX
// ============================================================================

#ifdef BUILD_CELL_INDEX

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= params.particleCount) return;
    
    uint hash = particleHashes[i];
    
    // Check if this is the start of a new cell
    if (i == 0) {
        cellStart[hash] = i;
    } else {
        uint prevHash = particleHashes[i - 1];
        if (hash != prevHash) {
            cellEnd[prevHash] = i;
            cellStart[hash] = i;
        }
    }
    
    // Handle last particle
    if (i == params.particleCount - 1) {
        cellEnd[hash] = i + 1;
    }
}

#endif

// ============================================================================
// STAGE 3: COMPUTE DENSITY AND PRESSURE
// ============================================================================

#ifdef COMPUTE_DENSITY

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= params.particleCount) return;
    
    uint i = particleIndices[idx];
    vec3 posA = particles[i].position;
    
    float density = 0.0;
    ivec3 cellA = getGridCell(posA);
    
    // Check 27 neighboring cells (including current cell)
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                ivec3 neighborCell = cellA + ivec3(dx, dy, dz);
                
                // Check bounds
                if (any(lessThan(neighborCell, ivec3(0))) || 
                    any(greaterThanEqual(neighborCell, ivec3(params.gridDim)))) {
                    continue;
                }
                
                uint hash = hashCell(neighborCell);
                uint start = cellStart[hash];
                uint end = cellEnd[hash];
                
                // Check all particles in this cell
                for (uint idx2 = start; idx2 < end; idx2++) {
                    uint j = particleIndices[idx2];
                    vec3 posB = particles[j].position;
                    vec3 diff = posA - posB;
                    float r2 = dot(diff, diff);
                    
                    if (r2 < params.smoothingRadius2) {
                        density += params.mass * poly6Kernel(r2);
                    }
                }
            }
        }
    }
    
    density = max(1e-6, density);
    sphData[i].density = density;
    sphData[i].pressure = params.pressureConstant * (density - params.restDensity);
}

#endif

// ============================================================================
// STAGE 4: COMPUTE FORCES
// ============================================================================

#ifdef COMPUTE_FORCES

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= params.particleCount) return;
    
    uint i = particleIndices[idx];
    vec3 posA = particles[i].position;
    vec3 velA = particles[i].velocity;
    float densityA = sphData[i].density;
    float pressureA = sphData[i].pressure;
    
    vec3 force = vec3(0.0);
    ivec3 cellA = getGridCell(posA);
    
    // Check 27 neighboring cells
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                ivec3 neighborCell = cellA + ivec3(dx, dy, dz);
                
                if (any(lessThan(neighborCell, ivec3(0))) || 
                    any(greaterThanEqual(neighborCell, ivec3(params.gridDim)))) {
                    continue;
                }
                
                uint hash = hashCell(neighborCell);
                uint start = cellStart[hash];
                uint end = cellEnd[hash];
                
                for (uint idx2 = start; idx2 < end; idx2++) {
                    uint j = particleIndices[idx2];
                    if (i == j) continue;
                    
                    vec3 posB = particles[j].position;
                    vec3 velB = particles[j].velocity;
                    float densityB = sphData[j].density;
                    float pressureB = sphData[j].pressure;
                    
                    vec3 diff = posA - posB;
                    float r2 = dot(diff, diff);
                    float r = sqrt(r2);
                    
                    if (r > 0.0 && r < params.smoothingRadius) {
                        vec3 rNorm = diff / r;
                        
                        // Pressure force
                        float pressureTerm = pressureA / (densityA * densityA) + 
                                            pressureB / (densityB * densityB);
                        vec3 pressureForce = -params.mass * pressureTerm * 
                                            spikyGradient(r, rNorm);
                        
                        // Viscosity force
                        vec3 viscosityForce = (1.0 / densityB) * (velB - velA) * 
                                             viscosityKernel(r);
                        
                        force += pressureForce + params.viscosityConstant * viscosityForce;
                    }
                }
            }
        }
    }
    
    sphData[i].force = force;
}

#endif

// ============================================================================
// STAGE 5: INTEGRATE
// ============================================================================

#ifdef INTEGRATE

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= params.particleCount) return;
    
    uint i = particleIndices[idx];
    
    vec3 pos = particles[i].position;
    vec3 vel = particles[i].velocity;
    vec3 force = sphData[i].force;
    float density = sphData[i].density;
    
    // Apply gravity and SPH forces
    vec3 gravity = vec3(0.0, params.gravity * params.mass, 0.0);
    vel += params.dt * (force / density + gravity);
    pos += params.dt * vel;
    
    // Boundary collisions
    if (pos.x < params.boxMin.x) {
        pos.x = params.boxMin.x;
        vel.x = abs(vel.x) * params.bounce;
    } else if (pos.x > params.boxMax.x) {
        pos.x = params.boxMax.x;
        vel.x = -abs(vel.x) * params.bounce;
    }
    
    if (pos.y < params.boxMin.y) {
        pos.y = params.boxMin.y;
        vel.y = abs(vel.y) * params.bounce;
    } else if (pos.y > params.boxMax.y) {
        pos.y = params.boxMax.y;
        vel.y = -abs(vel.y) * params.bounce;
    }
    
    if (pos.z < params.boxMin.z) {
        pos.z = params.boxMin.z;
        vel.z = abs(vel.z) * params.bounce;
    } else if (pos.z > params.boxMax.z) {
        pos.z = params.boxMax.z;
        vel.z = -abs(vel.z) * params.bounce;
    }
    
    particles[i].position = pos;
    particles[i].velocity = vel;
}

#endif