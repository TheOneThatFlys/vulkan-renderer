#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 UV;

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
};

layout(set = 2, binding = 0) uniform ModelUniforms {
    mat4 model;
};

void main() {
    gl_Position = projection * view * model * vec4(inPosition.xyz, 1.0);
    UV = vec2(inUV.x, 1.0 - inUV.y);
}