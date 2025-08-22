#version 450

layout(location = 0) in vec2 UV;

layout(set = 1, binding = 0) uniform sampler2D image;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 fragColor = texture(image, UV).rgb;
    // gamma correction
    outColor = vec4(pow(fragColor, vec3(1/2.2)), 1.0);
    // outColor = vec4(fragColor, 1.0);
}