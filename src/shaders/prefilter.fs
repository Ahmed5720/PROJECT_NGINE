#version 430 core

// shader used to convolve an environment map at different mipmapping scales to be used as texture for specular IBL
in vec3 WorldPos;
out vec4 FragColor;

uniform samplerCube environment;
uniform float roughness;
const float PI = 3.141592653;


vec3 importanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness*roughness;
	
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
	// from spherical coordinates to cartesian coordinates - halfway vector
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
	
	// from tangent-space H vector to world-space sample vector
	vec3 up          = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);
	
	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}


float RadicalInverse_VdC(uint bits) 
{
     bits = (bits << 16u) | (bits >> 16u);
     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

void main()
{
    vec3 N = normalize(WorldPos);
    vec3 R = N; // assumes that the reflection direction is simply the same as the viewing direction
    vec3 V = R;

    const uint SAMPLE_COUNT = 4096;
    float totalWeight = 0.0;
    vec3 prefilteredColor = vec3(0.0);
    // gathers random positions on the quad
    for (uint i = 0; i < SAMPLE_COUNT; i++)
    {   
        // produces random sample vector, note that learnopengl implementation uses a low discrepenancy sampler known as hammersley. compare with that 
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        // gets a half vector (normal of a microfacet) importance sampled based on roughness.
        vec3 H = importanceSampleGGX(Xi, N, roughness);
        // computers light direction based as reflection of view over the microfacet's normal (H)
        vec3 L = normalize(2.0 * dot(V,H) * H - V);
        
        float NdotL = max(dot(N,L), 0.0);
        if(NdotL > 0.0)
        {   
            // if we sample directly from the env map we would need a lot of samples in order not to get dotted patterns
            // to solve this we sample from the mipmaps based on the pdf and roughness instead
            // but we still get the same effect even after this?
            prefilteredColor += texture(environment, L).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;
    FragColor = vec4(prefilteredColor, 1.0);
}