#version 430 core


struct DirLight
{
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
struct PointLight
{
    vec3 position;
    
    float constant;
    float linear;
    float quadratic;  

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
  
    float constant;
    float linear;
    float quadratic;
  
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;       
};
struct Material
{
    sampler2D diffuse;
    sampler2D specular;
    vec3 diffuseColor;
    float shininess;
};

#define N_POINT_LIGHTS 4

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;

out vec4 FragColor;
uniform DirLight dirLight;
uniform PointLight pointLights[N_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform Material material;
uniform sampler2D shadowMap;
uniform vec3 viewPos;
uniform int numPointLights;
uniform int numSpotLights;
uniform bool hasTexture;
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float shadow);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
float calcShadow(vec4 FragPosLightSpace);
void main()
{
   
    // Normalize inputs
    vec3 normal = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

   // vec3 Directional = normalize(-dirLight.direction);
    float shadow = calcShadow(FragPosLightSpace);
    vec3 result = CalcDirLight(dirLight, normal, viewDir, shadow);
    for (int i = 0; i < numPointLights; ++i)
        result += CalcPointLight(pointLights[i], normal, FragPos, viewDir);
    if (numSpotLights > 0)
        result += CalcSpotLight(spotLight, normal, FragPos, viewDir);

    FragColor = vec4(result, 1.0);
}
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance    = length(light.position - fragPos);
    float denom = light.constant + light.linear * distance + light.quadratic * (distance * distance);
    float attenuation = 1.0 / max(denom, 0.0001);
    // combine results
    vec3 diffuseColor;
    vec3 specularColor;
    
    if (hasTexture)
    {
        diffuseColor = vec3(texture(material.diffuse, TexCoords));
        specularColor = vec3(texture(material.specular, TexCoords));
    }
    else
    {
        diffuseColor = material.diffuseColor;
        specularColor = material.diffuseColor; 
    }
    vec3 ambient = light.ambient * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * specularColor;

    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float shadow)
{
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // combine results
    vec3 diffuseColor;
    vec3 specularColor;
    
    if (hasTexture)
    {
        diffuseColor = vec3(texture(material.diffuse, TexCoords));
        specularColor = vec3(texture(material.specular, TexCoords));
    }
    else
    {
        diffuseColor = material.diffuseColor;
        specularColor = material.diffuseColor; 
    }
    vec3 ambient = light.ambient * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * specularColor;

    return (ambient + (1.0 - shadow) *  (diffuse + specular));
}
// calculates the color when using a spot light.
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float denom = light.constant + light.linear * distance + light.quadratic * (distance * distance);
    float attenuation = 1.0 / max(denom, 0.0001);
    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction)); 
    float epsilon = max(light.cutOff - light.outerCutOff, 0.0001);
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    // combine results

    vec3 diffuseColor;
    vec3 specularColor;
    
    if (hasTexture)
    {
        diffuseColor = vec3(texture(material.diffuse, TexCoords));
        specularColor = vec3(texture(material.specular, TexCoords));
    }
    else
    {
        diffuseColor = material.diffuseColor;
        specularColor = material.diffuseColor; 
    }
    vec3 ambient = light.ambient * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * specularColor;

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}
float calcShadow(vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    float bias = 0.005;
    //float bias = max(0.05 * (1.0 - dot(normal, Directional)), 0.005);  
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;

    return shadow;
}