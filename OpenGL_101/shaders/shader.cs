#version 430

layout (local_size_x = 1) in; 

layout(std430, binding = 0) buffer Pos
{
    vec4 Position [];
};
void main()
{
    uint idx = gl_GlobalInvocationID.X;
    vec3 p = Position[idx].xyz;
}