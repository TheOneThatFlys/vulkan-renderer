#include "Material.h"

#include "Pipeline.h"
#include "Renderer3D.h"
#include "VulkanEngine.h"

Material::Material(const Texture *base, const Texture *metallicRoughness, const Texture *ao, const Texture *normal)
    : m_base(base)
    , m_metallicRoughness(metallicRoughness)
    , m_ao(ao)
    , m_normal(normal)
{
    m_descriptorSet = VulkanEngine::getRenderer()->getPipeline()->createDescriptorSet(MATERIAL_SET_NUMBER);
    const std::array textures = {m_base, m_metallicRoughness, m_ao, m_normal};
    std::array<vk::DescriptorImageInfo, textures.size()> imageInfos;
    std::array<vk::WriteDescriptorSet, textures.size()> writeDescriptors;
    for (u32 i = 0; i < textures.size(); ++i) {
        imageInfos.at(i) = {
            .sampler = textures.at(i)->getSampler(),
            .imageView = textures.at(i)->getImage().getView(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        writeDescriptors.at(i)= {
            .dstSet = m_descriptorSet,
            .dstBinding = i,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfos.at(i)
        };
    }
    VulkanEngine::getDevice().updateDescriptorSets(writeDescriptors, nullptr);
}

void Material::use(const vk::raii::CommandBuffer &commandBuffer, const vk::raii::PipelineLayout& layout) const {
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, MATERIAL_SET_NUMBER, *m_descriptorSet, nullptr);
}
