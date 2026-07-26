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
    sampler2D diffuseMap;
    sampler2D normalMap;
    sampler2D roughnessMap;
    sampler2D aoMap;
    vec3 diffuseColor;
    float metallic;
    float roughness;
};

#define N_MAX_POINT_LIGHTS 16
#define N_MAX_SPOT_LIGHTS 16


in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;
out vec4 FragColor;

uniform int numSpotLights;
uniform int numPointLights;
uniform bool hasTexture;
uniform vec3 viewPos;
uniform sampler2D shadowMap;
uniform DirLight dirLight;
uniform PointLight pointLights[N_MAX_POINT_LIGHTS];
uniform SpotLight spotLights[N_MAX_SPOT_LIGHTS];
uniform Material material;
const float PI = 3.14159265359;
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 CalcDirLight(DirLight light, vec3 N, vec3 V, vec3 diffuse, vec3 F0, vec3 Lo, float roughness, float metallic, float shadow);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
float calcShadow(vec4 FragPosLightSpace, vec3 l, vec3 normal);

void main()
{
    // Normalize inputs
    vec3 normal = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

   vec3 d = normalize(-dirLight.direction);
   // no maps for now, F0, diffuse, normal, roughness 
    // if !hasTexture
    float roughness = material.roughness;
    vec3 diffuse = material.diffuseColor;
    float metallic = material.metallic;
    float ao = 1.0;
    //else ..
    float shadow = calcShadow(FragPosLightSpace, d, normal);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, material.diffuseColor, metallic);
    // out reflectance
    vec3 Lo = vec3(0.0);
    Lo += CalcDirLight(dirLight, normal, viewDir, diffuse, F0, Lo, roughness, metallic, shadow);
    // for (int i = 0; i < numPointLights; ++i)
    //     result += CalcPointLight(pointLights[i], normal, FragPos, viewDir);
    // for (int i = 0; i < numPointLights; ++i)
    //     result += CalcSpotLight(spotLights[i], normal, FragPos, viewDir);
    vec3 ambient = vec3(0.03) * diffuse * ao;
    vec3 color = ambient + (1 - shadow) * Lo;
    // vec3 color = ambient + Lo;

    // rienhard tonemapping
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); // gamma correction

    FragColor = vec4(color, 1.0);
}
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}
/*
 computes a ratio between spec and diffuse reflections
 F0 is surface reflection at zero incidence (when directly looking at surface)
 for metals this base reflection is tinted by their color
 for dielectrics we assume a fixed base reflection of 0.04
*/
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 CalcDirLight(DirLight light, vec3 N, vec3 V, vec3 diffuse, vec3 F0, vec3 Lo, float roughness, float metallic, float shadow)
{
    // calculate light radiance
    vec3 L = normalize(-light.direction);
    vec3 H = normalize(V + L);
    vec3 radiance = light.diffuse;

    // cook-torrence brdf
    
    float NDF = DistributionGGX(N,H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H,V), 0.0), F0);

    vec3 kS = F; 
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // metals have no diffuse component

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N,V), 0.0) * max(dot(N,L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    float NdotL = max(dot(N,L), 0.0);
    return ((kD * diffuse / PI) + specular) * radiance * NdotL;
}

float calcShadow(vec4 fragPosLightSpace, vec3 l, vec3 normal)
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
    // float bias = 0.0005;
    float bias = max(0.005 * (1.0 - dot(normal, l)), 0.0005);  
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
