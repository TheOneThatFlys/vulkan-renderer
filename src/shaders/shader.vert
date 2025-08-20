#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 UV;

layout(set = 0, binding = 0) uniform MatrixUniforms {
    mat4 model;
    mat4 view;
    mat4 projection;
} matrices;

void main() {
    gl_Position = matrices.projection * matrices.view * matrices.model * vec4(inPosition.xyz, 1.0);
    UV = inUV;
}