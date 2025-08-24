#pragma once

#include "ECS.h"

#include <vulkan/vulkan_raii.hpp>
#include "Pipeline.h"
#include "UniformBufferBlock.h"

struct FrameUniforms {
    glm::mat4 view;
    glm::mat4 projection;
};

struct ModelUniforms {
    glm::mat4 transform;
};

class Renderer3D : public ECS::System {
public:
    Renderer3D(VulkanEngine *engine, const Pipeline *pipeline);
    void render(const vk::raii::CommandBuffer& buffer, const CameraController* camera);

private:
    VulkanEngine* m_engine;
    const Pipeline* m_pipeline;

    vk::raii::DescriptorSet m_frameDescriptor;
    vk::raii::DescriptorSet m_modelDescriptor;

    UniformBufferBlock<FrameUniforms> m_frameUniforms;
    UniformBufferBlock<ModelUniforms> m_modelUniforms;
};
