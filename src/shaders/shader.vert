#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNorm;
layout(location = 3) in vec3 inTang;

layout(location = 0) out vec2 UV;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec3 FragPos;
layout(location = 3) out mat3 TBN;

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
};

layout(set = 2, binding = 0) uniform ModelUniforms {
    mat4 model;
    mat4 normalMatrix;
};

void main() {
    gl_Position = projection * view * model * vec4(inPosition.xyz, 1.0);
    UV = vec2(inUV.x, 1.0 - inUV.y);
    Normal = mat3(normalMatrix) * inNorm;
    FragPos = vec3(model * vec4(inPosition, 1));

    vec3 T = normalize(vec3(model * vec4(inTang, 0.0)));
    vec3 N = normalize(vec3(model * vec4(inNorm, 0.0)));
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);
}