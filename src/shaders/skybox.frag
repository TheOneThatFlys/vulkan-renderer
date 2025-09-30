#version 460

layout(location = 0) out vec4 FragColour;

layout(location = 0) in vec3 UV;

uniform layout(set = 0, binding = 1) samplerCube skybox;

void main() {
    FragColour = texture(skybox, UV);
    // FragColour = vec4(UV, 1.0f);
}