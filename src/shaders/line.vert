#version 460

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outColour;

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
};

layout(set = 2, binding = 0) uniform ModelUniforms {
    mat4 model;
    vec3 colour;
};

void main() {
    gl_Position = projection * view * model * vec4(inPosition.xyz, 1.0);
    outColour = colour;
}