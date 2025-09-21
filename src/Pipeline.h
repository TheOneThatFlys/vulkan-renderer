#pragma once

#include <unordered_map>
#include <vulkan/vulkan_raii.hpp>
#include "Common.h"

class VulkanEngine;
class Pipeline {
public:
    class Builder {
    public:
        explicit Builder(VulkanEngine* engine);
        Builder& setVertexInfo(const vk::VertexInputBindingDescription& bindings, const std::vector<vk::VertexInputAttributeDescription>& attributes);
        Builder& addShaderStage(std::string path);
        Builder& addBinding(u32 set, u32 binding, vk::DescriptorType type, vk::ShaderStageFlagBits stage);
        Builder& addDynamicState(vk::DynamicState state);
        Builder& setPolygonMode(vk::PolygonMode polygonMode);
        Builder& setTopology(vk::PrimitiveTopology topology);
        Builder& disableDepthTest();
        Builder& setSamples(vk::SampleCountFlagBits samples);
        std::unique_ptr<Pipeline> create();
    private:

        VulkanEngine* m_engine;
        std::unordered_map<vk::ShaderStageFlagBits, vk::raii::ShaderModule> m_shaders;
        vk::PipelineVertexInputStateCreateInfo m_vertexInputInfo;
        vk::VertexInputBindingDescription m_bindings;
        std::vector<vk::VertexInputAttributeDescription> m_attributes;

        vk::PipelineInputAssemblyStateCreateInfo m_inputAssembly;
        std::vector<vk::DynamicState> m_dynamicStates;
        vk::PipelineViewportStateCreateInfo m_viewportState;
        vk::PipelineRasterizationStateCreateInfo m_rasterizer;
        vk::PipelineMultisampleStateCreateInfo m_multisampling;
        vk::PipelineDepthStencilStateCreateInfo m_depthStencil;
        vk::PipelineColorBlendAttachmentState m_colorBlendAttachment;
        vk::PipelineColorBlendStateCreateInfo m_colorBlending;

        std::array<std::vector<vk::DescriptorSetLayoutBinding>, 4> m_descriptorBindings;
    };

    Pipeline(VulkanEngine* engine, vk::raii::Pipeline pipeline, vk::raii::PipelineLayout layout, std::vector<vk::raii::DescriptorSetLayout> descriptorSetLayouts);
    vk::raii::DescriptorSet createDescriptorSet(u32 set) const;
    const vk::raii::Pipeline& getPipeline() const;
    const vk::raii::PipelineLayout& getLayout() const;
    const vk::raii::DescriptorSetLayout& getDescriptorLayout(u32 set) const;

private:
    VulkanEngine* m_engine = nullptr;
    vk::raii::Pipeline m_pipeline = nullptr;
    vk::raii::PipelineLayout m_layout = nullptr;
    std::vector<vk::raii::DescriptorSetLayout> m_descriptorLayouts;
};
