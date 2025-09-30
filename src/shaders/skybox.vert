#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNorm;
layout(location = 3) in vec3 inTang;

layout(location = 0) out vec3 UV;

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
};

void main() {
    gl_Position = (projection * mat4(mat3(view)) * vec4(inPosition.xyz, 1.0)).xyww;
    UV = inPosition;
}