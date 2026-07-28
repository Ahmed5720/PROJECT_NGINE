#version 430 core

// we no longer make use of ambient and specular components of lights in the PBR pipeline. is that standard?
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
    sampler2D metallicMap;
    sampler2D aoMap;
    vec3 diffuseColor;
    float metallic;
    float roughness;
    float alpha;
    bool emissive;
};

#define N_MAX_POINT_LIGHTS 16
#define N_MAX_SPOT_LIGHTS 16


in vec2 uv;
in vec3 Normal;
in vec3 vTangent;
in vec3 vBitangent;
in vec3 FragPos;
in vec4 FragPosLightSpace;
out vec4 FragColor;

uniform int numSpotLights;
uniform int numPointLights;
uniform bool hasDiffuseTex;
uniform bool hasRoughnessTex;
uniform bool hasNormalTex;
uniform bool hasAoTex;
uniform bool hasMetallicTex;
uniform vec3 viewPos;
uniform sampler2D shadowMap;
uniform samplerCube environment;
uniform float ambientStrength;
uniform DirLight dirLight;
uniform PointLight pointLights[N_MAX_POINT_LIGHTS];
uniform SpotLight spotLights[N_MAX_SPOT_LIGHTS];
uniform Material material;
const float PI = 3.14159265359;
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 CalcDirLight(DirLight light, vec3 N, vec3 V, vec3 diffuse, vec3 F0, float roughness, float metallic);
float calcShadow(vec4 FragPosLightSpace, vec3 l, vec3 normal);
vec3 CalcPointLight(PointLight pl, vec3 fragPos, vec3 N, vec3 V, vec3 diffuse, vec3 F0, float roughness, float metallic);

// TBD
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main()
{   
    if(material.emissive)
    {
        FragColor = vec4(material.diffuseColor, 1.0);
        return;
    }

    // Normalize inputs
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 d = normalize(-dirLight.direction);
   // no maps for now, F0, diffuse, normal, roughness 
    // if !hasTexture
    vec3 diffuse = hasDiffuseTex ? vec3(texture(material.diffuseMap, uv)) : material.diffuseColor;
    float roughness = hasRoughnessTex ? texture(material.roughnessMap, uv).r : material.roughness;
    float metallic = hasMetallicTex ? texture(material.metallicMap, uv).r : material.metallic;
    float ao = hasAoTex ? texture(material.aoMap, uv).r : ambientStrength;
    vec3 normal = normalize(Normal);
    if(hasNormalTex)
    {
        vec3 tangentNormal = normalize(texture(material.normalMap, uv).rgb * 2.0 - 1.0);

        vec3 N = normal;
        vec3 T = normalize(vTangent);
        vec3 B = normalize(vBitangent);
        mat3 TBN = mat3(T, B, N);
        // now normal is transformed to world space
        normal = normalize(TBN * tangentNormal); 
        //normal = normalize(tangentNormal);
    }
   
        

    //else ..
    float shadow = calcShadow(FragPosLightSpace, d, normal);
    //float shadow = 0; 
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, material.diffuseColor, metallic);
    // out reflectance
    vec3 Lo = vec3(0.0);
    vec3 pLight = vec3(0.0);
    Lo += CalcDirLight(dirLight, normal, viewDir, diffuse, F0, roughness, metallic);
    for (int i = 0; i < numPointLights; ++i)
         pLight += CalcPointLight(pointLights[i], FragPos, normal, viewDir, diffuse, F0, roughness, metallic);
    // for (int i = 0; i < numPointLights; ++i)
    //     result += CalcSpotLight(spotLights[i], normal, FragPos, viewDir);
  //  vec3 ambient = vec3(0.03) * diffuse * ao;

    vec3 kS = fresnelSchlick(max(dot(normal, viewDir), 0.0), F0);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    vec3 irradiance = texture(environment, normal).rgb;
    vec3 diffuseLight = irradiance * diffuse;
    vec3 ambient = (kD * diffuseLight) * ao;
    
  //  vec3 ambient = texture(environment, normal).rgb * 0.1f;
    vec3 color = pLight + ambient + (1 - shadow) * Lo;
    // vec3 color = ambient + Lo;

    // rienhard tonemapping
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); // gamma correction

    FragColor = vec4(color, material.alpha);
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
vec3 CalcDirLight(DirLight light, vec3 N, vec3 V, vec3 diffuse, vec3 F0,float roughness, float metallic)
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

vec3 CalcPointLight(PointLight pl, vec3 fragPos, vec3 N, vec3 V, vec3 diffuse, vec3 F0, float roughness, float metallic)
{
    // calculate light radiance
    vec3 L = normalize(pl.position - fragPos);
    vec3 H = normalize(V + L);

    float distance    = length(pl.position - fragPos);
    float denom = pl.constant + pl.linear * distance + pl.quadratic * (distance * distance);
    float attenuation = 1.0 / max(denom, 0.0001);

    vec3 radiance = pl.diffuse * attenuation;

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
    float bias = max(0.0005 * (1.0 - dot(normal, l)), 0.0005);  
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
