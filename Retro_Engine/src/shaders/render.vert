#version 430 core

layout(location = 0) in vec3 instPos;     // from particles buffer
layout(location = 1) in vec4 instColor;   // from particles buffer
layout(location = 2) in vec2 corner;      // from quad buffer

layout(std140, binding = 1) uniform CameraUniforms {
    mat4 mvp;
    vec4 right; // xyz used
    vec4 up;    // xyz used
};

out VS_OUT {
    vec4 color;
    vec2 corner;
} vs_out;

void main() {
    float size = 0.02;
    vec3 worldPos = instPos
        + right.xyz * (corner.x * size)
        + up.xyz    * (corner.y * size);

    gl_Position = mvp * vec4(worldPos, 1.0);
    vs_out.color = instColor;
    vs_out.corner = corner;
}
