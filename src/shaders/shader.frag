#version 450

layout(location = 0) in vec2 UV;

layout(set = 0, binding = 1) uniform sampler2D image;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 fragColor = texture(image, UV).rgb;
    // gamma correction
    outColor = vec4(pow(fragColor, vec3(1/2.2)), 1.0);
}