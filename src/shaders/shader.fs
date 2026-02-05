#version 430 core

in vec2 vUV;
in vec3 vNormalWS;
in vec3 FragPos;
uniform sampler2D uTex0;
uniform vec3 uLightDirection;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float specularStrength;
uniform float ambientStrength;
out vec4 FragColor;

void main()
{
    vec3 albedo = texture(uTex0, vUV).rgb;
    
    // Normalize inputs
    vec3 normal = normalize(vNormalWS);
    vec3 lightDir = normalize(uLightDirection);
    
    //phong shading
    float diff = max(dot(normal, lightDir), 0.1);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    vec3 diffuse = diff * lightColor;
    vec3 ambient = ambientStrength * lightColor;
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * albedo;
    FragColor = vec4(result, 1.0);
}