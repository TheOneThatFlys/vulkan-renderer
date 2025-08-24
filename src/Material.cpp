#include "Material.h"

#include "Pipeline.h"
#include "VulkanEngine.h"

Material::Material(const VulkanEngine* engine, const Texture *base) : m_base(base) {
    m_descriptorSet = engine->getPipeline()->createDescriptorSet(MATERIAL_SET_NUMBER);
    vk::DescriptorImageInfo imageInfo = {
        .sampler = m_base->getSampler(),
        .imageView = m_base->getImageView(),
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };
    vk::WriteDescriptorSet samplerDescriptorWrite = {
        .dstSet = m_descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &imageInfo
    };
    engine->getDevice().updateDescriptorSets(samplerDescriptorWrite, nullptr);
}

void Material::use(const vk::raii::CommandBuffer &commandBuffer, const vk::raii::PipelineLayout& layout) const {
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, MATERIAL_SET_NUMBER, *m_descriptorSet, nullptr);
}
