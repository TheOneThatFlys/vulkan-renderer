#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "ECS.h"
#include "LightSystem.h"
#include "Pipeline.h"
#include "UniformBufferBlock.h"

struct FrameUniforms {
    glm::mat4 view;
    glm::mat4 projection;
};

struct ModelUniforms {
    glm::mat4 transform;
    glm::mat4 normal;
};

struct FragFrameData {
    glm::vec3 cameraPosition;
    std::array<LightSystem::PointLightFragData, LightSystem::MAX_LIGHTS> lights;
    u32 nLights;
};

class Renderer3D : public ECS::System {
public:
    Renderer3D(VulkanEngine *engine, const Pipeline *pipeline);
    void render(const vk::raii::CommandBuffer& buffer);

private:
    VulkanEngine* m_engine;
    const Pipeline* m_pipeline;

    ECS::Entity m_camera;

    vk::raii::DescriptorSet m_frameDescriptor;
    vk::raii::DescriptorSet m_modelDescriptor;

    UniformBufferBlock<FrameUniforms> m_frameUniforms;
    DynamicUniformBufferBlock<ModelUniforms> m_modelUniforms;
    UniformBufferBlock<FragFrameData> m_fragFrameUniforms;
};
