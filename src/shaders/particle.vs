#version 430 core


uniform mat4 view;
uniform mat4 projection;
uniform float pointSize = 10.0;

out vec4 vColor;
out float vLife;

struct Particle {
    vec4 position;   // xyz + pad
    vec4 color;      // rgba
    vec4 velocity;   // xyz + pad
    float life;
    vec3 _padLife;   // pad to 16 bytes
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

void main()
{
    uint id = uint(gl_VertexID);
    Particle p = particles[id];

    gl_Position = projection * view * vec4(p.position.xyz, 1.0);
    gl_PointSize = pointSize; // * clamp(p.life, 0.0, 1.0);
    vColor = vec4(p.color.rgb, 1.0);// p.color.a * clamp(p.life, 0.0, 1.0));
}