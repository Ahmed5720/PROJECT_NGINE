#version 430

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in; // 1 threads per work group 
layout(r32f, binding = 0) uniform image2D out_tex;

void main()
{
    ivec2 pos = ivec3(gl_GlobalInvocationID.xy);
    if (idx >= Particles.Length()) return;

    //get value stored at pos in tex
    float in_val = imageLoad(out_tex, pos).r;

    //ser value stored at pos in tex
    imageStore(out_tex, pos, vec4(in_val + 1, 0.0, 0.0, 0.0));

}