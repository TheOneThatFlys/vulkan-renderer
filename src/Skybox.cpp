#include "Skybox.h"

#include <vulkan/vulkan_format_traits.hpp>

#include "Renderer3D.h"
#include "VulkanEngine.h"

Skybox::Skybox(const std::array<unsigned char *, 6> &pixels, const u32 width, const u32 height, const vk::Format format) {
    assert(vk::componentCount(format) == 4 && vk::componentBits(format, 0) == 8);
    constexpr u32 bitdepth = 4;
    vk::DeviceSize imageSize = width * height * bitdepth;

    auto [stagingBuffer, stagingBufferMemory] = VulkanEngine::createBuffer(imageSize * 6, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    void* data = stagingBufferMemory.mapMemory(0, imageSize * 6);
    for (u32 i = 0; i < 6; ++i) {
        memcpy(static_cast<unsigned char*>(data) + i * imageSize, pixels[i], imageSize);
    }
    stagingBufferMemory.unmapMemory();

    m_image = std::make_unique<Image>(ImageCreateInfo{
        .width = width,
        .height = height,
        .format = format,
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .arrayLayers = 6,
        .viewType = vk::ImageViewType::eCube,
        .flags = vk::ImageCreateFlagBits::eCubeCompatible
    });

    const auto commandBuffer = VulkanEngine::beginSingleCommand();

    m_image->changeLayout(commandBuffer, {
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .arrayLayers = 6
    });

    // copy staging buffer data to image
    vk::BufferImageCopy copyRegion = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 6
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    commandBuffer.copyBufferToImage(stagingBuffer, m_image->getImage(), vk::ImageLayout::eTransferDstOptimal, copyRegion);

    m_image->changeLayout(commandBuffer, {
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .arrayLayers = 6
    });
    VulkanEngine::endSingleCommand(commandBuffer);

    vk::SamplerCreateInfo samplerCreateInfo = {
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = VulkanEngine::getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .unnormalizedCoordinates = vk::False
    };

    m_sampler = vk::raii::Sampler(VulkanEngine::getDevice(), samplerCreateInfo);
}

void Skybox::addToSet(const vk::raii::DescriptorSet &set, const u32 binding) const {
    const vk::DescriptorImageInfo imageInfo = {
        .sampler = m_sampler,
        .imageView = m_image->getView(),
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    const vk::WriteDescriptorSet writeInfo = {
        .dstSet = set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &imageInfo
    };
    VulkanEngine::getDevice().updateDescriptorSets(writeInfo, nullptr);
}

const Image & Skybox::getImage() const {
    return *m_image;
}

const vk::raii::Sampler & Skybox::getSampler() const {
    return m_sampler;
}
