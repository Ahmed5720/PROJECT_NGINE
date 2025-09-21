#version 430

const int NUM_THREADS = 64;

//buffer bindings
layout(std430, binding = 0) restrict buffer PositionBuffer
{
	vec3 positions[];
};

layout(std430, binding = 1) restrict buffer PredictedPositionBuffer
{
	vec3 predictedPositions[];
};
layout(std430, binding = 1) restrict buffer VelocityBuffer
{
	vec3 velocities[];
};
layout(std430, binding = 1) restrict buffer DensityBuffer
{
	vec3 densities[];
};

layout(std140, binding = 0) uniform SimulationParams
{
	uint numParticles;
	float gravity;
	float deltaTime;
	float collisionDamping;
	float targetDensity;
	float pressureMultiplier;
	// float nearPressureMultiplier
	// float viscosityStrength
	//float edgeForce;
	//float edgeForceDst;
	vec3 boundingBox;
	vec3 center;
};
