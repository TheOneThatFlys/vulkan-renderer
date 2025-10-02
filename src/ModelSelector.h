#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "ECS.h"
#include "UniformBufferBlock.h"

class ModelSelector : public IUpdatable {
public:
    explicit ModelSelector(vk::Extent2D extent);
    ~ModelSelector() override = default;
    void update(float) override;

    void enable();
    void disable();

    void setExtent(vk::Extent2D extent);
    ECS::Entity getSelected() const;

    constexpr vk::Format getTextureFormat() const;

private:

    struct SelectorFrameUniform {
        glm::mat4 view;
        glm::mat4 projection;
    };

    struct SelectorModelUniform {
        glm::mat4 transform;
        ECS::Entity ID;
    };

    ECS::Entity calculateSelectedEntity();
    void createAttachments();

    vk::Extent2D m_extent;

    bool m_enabled = false;
    ECS::Entity m_selected = ECS::NULL_ENTITY;

    std::unique_ptr<Pipeline> m_pipeline;

    std::unique_ptr<Image> m_colorImage;
    std::unique_ptr<Image> m_depthImage;

    vk::raii::Buffer m_outputBuffer = nullptr;
    vk::raii::DeviceMemory m_outputBufferMemory = nullptr;

    vk::raii::DescriptorSet m_frameDescriptor = nullptr;
    vk::raii::DescriptorSet m_modelDescriptor = nullptr;

    UniformBufferBlock<SelectorFrameUniform> m_frameUniforms;
    DynamicUniformBufferBlock<SelectorModelUniform> m_modelUniforms;
};
