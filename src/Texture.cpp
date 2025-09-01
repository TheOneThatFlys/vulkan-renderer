#include "Texture.h"

#include <vulkan/vulkan_format_traits.hpp>

#include "VulkanEngine.h"

Texture::Texture(const VulkanEngine *engine, const unsigned char *pixels, u32 width, u32 height, vk::Format format, const SamplerInfo& samplerInfo) {
    assert(vk::componentCount(format) == 4 && vk::componentBits(format, 0) == 8);
    constexpr u32 bitdepth = 4;
    vk::DeviceSize imageSize = width * height * bitdepth;

    auto [stagingBuffer, stagingBufferMemory] = engine->createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    std::tie(m_image, m_imageMemory) = engine->createImage(
        width, height,
        format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    // transition image layout for optimal copying
    engine->transitionImageLayout(m_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    // copy staging buffer data to image
    engine->copyBufferToImage(stagingBuffer, m_image, width, height);
    // transition to format optimal for shader reads
    engine->transitionImageLayout(m_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::ImageViewCreateInfo viewInfo = {
        .image = m_image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    m_imageView = vk::raii::ImageView(engine->getDevice(), viewInfo);

    vk::SamplerCreateInfo samplerCreateInfo = {
        .magFilter = samplerInfo.magFilter,
        .minFilter = samplerInfo.minFilter,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = samplerInfo.wrapU,
        .addressModeV = samplerInfo.wrapV,
        .addressModeW = samplerInfo.wrapW,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = engine->getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .unnormalizedCoordinates = vk::False
    };

    m_sampler = vk::raii::Sampler(engine->getDevice(), samplerCreateInfo);
}

const vk::raii::Image & Texture::getImage() const {
    return m_image;
}

const vk::raii::DeviceMemory & Texture::getImageMemory() const {
    return m_imageMemory;
}

const vk::raii::ImageView & Texture::getImageView() const {
    return m_imageView;
}

const vk::raii::Sampler & Texture::getSampler() const {
    return m_sampler;
}
