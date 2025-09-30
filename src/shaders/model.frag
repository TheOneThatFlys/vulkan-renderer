#version 460

#define PI 3.1415926535897932384626433
#define BRDF_EPSILON 0.000001

#define MAX_LIGHTS 4
#define DIELECTRIC_F0 0.04

struct BRDFParams {
    vec3 V;
    vec3 F0;
    vec3 albedo;
    float roughness;
    float metallic;
    vec3 normal;
};

struct PointLight {
    vec3 position;
    vec3 colour;
    float strength;
};

layout(location = 0) in vec2 UV;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec3 FragPos;
layout(location = 3) in mat3 TBN;

layout(location = 0) out vec4 OutColor;

layout(set = 1, binding = 0) uniform sampler2D baseColourTexture;
layout(set = 1, binding = 1) uniform sampler2D metallicRoughnessTexture;
layout(set = 1, binding = 2) uniform sampler2D aoTexture;
layout(set = 1, binding = 3) uniform sampler2D normalTexture;

layout(std140, set = 0, binding = 1) uniform FrameData {
    vec3 cameraPosition;
    PointLight lights[MAX_LIGHTS];
    int nLights;
    float far;
    float fog;
};

vec3 fresnelSchlick(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdH = max(dot(N, H), 0.0);
    float NdH2 = NdH * NdH;

    float numerator = a2;
    float temp = (NdH2) * (a2 - 1.0) + 1.0;
    float denominator = PI * temp * temp;
    return numerator / denominator;
}

float geometrySchlickGGX(float NdV, float roughness)
{
    float r = roughness + 1.0;
    float k = r*r / 8.0;
    float denominator = NdV * (1 - k) + k;
    return NdV / denominator;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = geometrySchlickGGX(NdotV, roughness);
    float ggx1  = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 pointLightContribution(PointLight pLight, BRDFParams p)
{
    vec3 L = normalize(pLight.position - FragPos);
    vec3 H = normalize(p.V + L);

    float distance = length(pLight.position - FragPos);
    float attenuation = pLight.strength / (distance * distance);
    vec3 radiance = pLight.colour * attenuation;

    vec3 F = fresnelSchlick(max(dot(H, p.V), 0.0), p.F0, p.roughness);
    float NDF = distributionGGX(p.normal, H, p.roughness);
    float G = geometrySmith(p.normal, p.V, L, p.roughness);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(p.normal, p.V), 0.0) * max(dot(p.normal, L), 0.0) + BRDF_EPSILON;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - p.metallic);

    float NdL = max(dot(p.normal, L), 0.0);
    return (kD * p.albedo / PI + specular) * radiance * NdL;
}

void main() {
    vec4 albedoAlpha = texture(baseColourTexture, UV).rgba;
    vec3 albedo = albedoAlpha.rgb;
    vec2 metallicRoughness = texture(metallicRoughnessTexture, UV).rg;
    float ao = texture(aoTexture, UV).r;
    vec3 normal = texture(normalTexture, UV).rgb;
    normal = normal * 2.0 - vec3(1.0);
    normal = normalize(TBN * normal);
    vec3 V = normalize(cameraPosition - FragPos);

    vec3 F0 = vec3(DIELECTRIC_F0);
    F0 = mix(F0, albedo, metallicRoughness.r);

    BRDFParams params;
    params.V = V;
    params.F0 = F0;
    params.albedo = albedo;
    params.roughness = metallicRoughness.g;
    params.metallic = metallicRoughness.r;
    params.normal = normal;

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < nLights; i++)
    {
        Lo += pointLightContribution(lights[i], params);
    }

    vec3 ambient = vec3(1.0) * albedo * ao;
    vec3 result = ambient + Lo;

    float distanceToFrag = length(FragPos - cameraPosition);
    float fogFactor = (distanceToFrag - (far - fog)) / fog;
    fogFactor = 1 - clamp(fogFactor, 0, 1);

    OutColor = vec4(pow(result, vec3(1/2.2)), fogFactor);
}