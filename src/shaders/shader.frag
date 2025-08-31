#version 450

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
    float alpha;
};

struct PointLight {
    vec3 position;
    vec3 colour;
    float strength;
};

layout(location = 0) in vec2 UV;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec3 FragPos;

layout(location = 0) out vec4 OutColor;

layout(set = 1, binding = 0) uniform sampler2D baseColourTexture;

layout(std140, set = 0, binding = 1) uniform FrameData {
    vec3 cameraPosition;
    PointLight lights[MAX_LIGHTS];
    int nLights;
};

void main() {
    vec3 normal = normalize(Normal);
    vec3 albedo = texture(baseColourTexture, UV).rgb;

    vec3 ambient = 0.1 * lights[0].colour;
    vec3 lightDirection = normalize(lights[0].position - FragPos);
    float diffuseFactor = max(dot(lightDirection, normal), 0.0);
    vec3 diffuse = diffuseFactor * lights[0].colour;
    vec3 fragColor = (ambient + diffuse) * albedo;

    // gamma correction
    OutColor = vec4(pow(fragColor, vec3(1/2.2)), 1.0);
}