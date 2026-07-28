#pragma once
#include <cmath>   // cosf, M_PI
// Sizes must match the #define constants in phong.frag
static constexpr int MAX_POINT_LIGHTS = 8;
static constexpr int MAX_SPOT_LIGHTS  = 4;

// DirectionalLight
//   Maps to uniform `DirLight dirLight` in the shader.
//   `direction` points TOWARD the light in world space (normalise before use).
//   ambient/diffuse/specular are pre-multiplied colour×intensity
struct DirectionalLight {
    float direction[3] = { 0.0f, -1.0f,  0.5f };
    float ambient[3]   = { 0.05f, 0.05f, 0.05f };
    float diffuse[3]   = { 0.8f,  0.8f,  0.8f  };
    float specular[3]  = { 0.5f,  0.5f,  0.5f  };
    bool  castsShadow  = false;   // reserved for ShadowPass
};
// PointLight
//   Maps to uniform `PointLight pointLights[N_POINT_LIGHTS]`.
//   Attenuation formula: 1 / (constant + linear*d + quadratic*d²)
//   Typical values for a ~20 unit radius light:
//     constant=1.0, linear=0.09, quadratic=0.032
struct PointLight {
    float position[3]  = { 0.0f, 2.0f, 0.0f };
    float constant     = 1.0f;
    float linear       = 0.09f;
    float quadratic    = 0.032f;
    float ambient[3]   = { 0.05f, 0.05f, 0.05f };
    float diffuse[3]   = { 0.8f,  0.8f,  0.8f  };
    float specular[3]  = { 1.0f,  1.0f,  1.0f  };
    bool  enabled      = true;
};

// SpotLight
//   Maps to uniform `SpotLight spotLights[N_SPOT_LIGHTS]`.
//   cutOff / outerCutOff are stored as cosines (pre-computed from degrees)
//   Use SpotLight::setAngles(inner, outer) to set them from degree values.
struct SpotLight {
    float position[3]  = { 0.0f, 4.0f, 0.0f };
    float direction[3] = { 0.0f, -1.0f, 0.0f };
    float cutOff       = 0.9763f;   // cos(12.5°)
    float outerCutOff  = 0.9659f;   // cos(15.0°)
    float constant     = 1.0f;
    float linear       = 0.09f;
    float quadratic    = 0.032f;
    float ambient[3]   = { 0.0f,  0.0f,  0.0f  };
    float diffuse[3]   = { 1.0f,  1.0f,  1.0f  };
    float specular[3]  = { 1.0f,  1.0f,  1.0f  };
    bool  enabled      = true;

    void setAngles(float innerDeg, float outerDeg) {
        const float deg2rad = 3.14159265f / 180.0f;
        cutOff      = cosf(innerDeg * deg2rad);
        outerCutOff = cosf(outerDeg * deg2rad);
    }
};

// LightEnvironment
//   Owns all lights in the scene.  The pipeline reads this and uploads
//   uniforms each frame.
struct LightEnvironment {
    DirectionalLight sun;
    float ambience = 0.5f;
    PointLight pointLights[MAX_POINT_LIGHTS];
    int        numPointLights = 0;

    SpotLight  spotLights[MAX_SPOT_LIGHTS];
    int        numSpotLights  = 0;

    int addPointLight(const PointLight& l) {
        if (numPointLights >= MAX_POINT_LIGHTS) return -1;
        pointLights[numPointLights] = l;
        return numPointLights++;
    }

    int addSpotLight(const SpotLight& l) {
        if (numSpotLights >= MAX_SPOT_LIGHTS) return -1;
        spotLights[numSpotLights] = l;
        return numSpotLights++;
    }
};