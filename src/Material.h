#pragma once

#include "Texture.h"

class Material {
public:
    Material(const VulkanEngine* engine, const Texture* base);
    void use(const vk::raii::CommandBuffer& commandBuffer, const vk::raii::PipelineLayout& layout) const;

private:
    const Texture* m_base;
    vk::raii::DescriptorSet m_descriptorSet = nullptr;
};