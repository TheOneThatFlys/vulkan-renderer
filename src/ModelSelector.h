#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "ECS.h"
#include "UniformBufferBlock.h"

class VulkanEngine;
class Pipeline;

class ModelSelector : public IUpdatable {
public:
    explicit ModelSelector(VulkanEngine* engine, vk::Extent2D extent);
    ~ModelSelector() override = default;
    void update(float) override;

    void enable();
    void disable();

    void setExtent(vk::Extent2D extent);

    constexpr vk::Format getTextureFormat() const;

    ECS::Entity calculateSelectedEntity();
private:
    struct SelectorFrameUniform {
        glm::mat4 view;
        glm::mat4 projection;
    };

    struct SelectorModelUniform {
        glm::mat4 transform;
        ECS::Entity ID;
    };

    void createAttachments();

    VulkanEngine* m_engine;

    vk::Extent2D m_extent;

    bool m_enabled = false;
    ECS::Entity m_selected = ECS::NULL_ENTITY;

    std::unique_ptr<Pipeline> m_pipeline;

    vk::raii::Image m_colourImage = nullptr;
    vk::raii::DeviceMemory m_colourImageMemory = nullptr;
    vk::raii::ImageView m_colourImageView = nullptr;

    vk::raii::Image m_depthImage = nullptr;
    vk::raii::DeviceMemory m_depthImageMemory = nullptr;
    vk::raii::ImageView m_depthImageView = nullptr;

    vk::raii::Buffer m_outputBuffer = nullptr;
    vk::raii::DeviceMemory m_outputBufferMemory = nullptr;

    vk::raii::DescriptorSet m_frameDescriptor = nullptr;
    vk::raii::DescriptorSet m_modelDescriptor = nullptr;

    UniformBufferBlock<SelectorFrameUniform> m_frameUniforms;
    DynamicUniformBufferBlock<SelectorModelUniform> m_modelUniforms;
};
