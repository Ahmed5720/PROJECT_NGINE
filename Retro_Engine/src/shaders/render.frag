#version 430 core

in VS_OUT {
    vec4 color;
    vec2 corner;
} fs_in;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 c = fs_in.color;
    c.a *= max(1.0 - length(fs_in.corner), 0.0);
    outColor = c;
}
