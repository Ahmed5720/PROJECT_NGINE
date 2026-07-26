#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;
out vec2 uv;
out vec3 Normal;
out vec3 FragPos;
out vec4 FragPosLightSpace;
out vec3 vTangent;
out vec3 vBitangent;

 
void main()
{   
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos = vec3(model * vec4(aPos, 1.0));
    FragPosLightSpace = lightSpace * vec4(FragPos, 1.0);
    // Calculate normal matrix (transpose(inverse(model)) for non-uniform scaling)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    Normal = normalize(normalMatrix * aNormal);
    uv = aUV;
}