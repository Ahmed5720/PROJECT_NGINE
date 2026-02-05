#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 vUV;
out vec3 vNormalWS;
out vec3 FragPos;

void main()
{   
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos = vec3(model * vec4(aPos, 1.0));
    // Calculate normal matrix (transpose(inverse(model)) for non-uniform scaling)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vNormalWS = normalize(normalMatrix * aNormal);
    
    vUV = aUV;
}