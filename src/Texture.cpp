#include "Texture.h"

#include <vulkan/vulkan_format_traits.hpp>

#include "VulkanEngine.h"

Texture::Texture(const VulkanEngine *engine, const unsigned char *pixels, u32 width, u32 height, vk::Format format, const SamplerInfo& samplerInfo) : m_width(width), m_height(height) {
    assert(vk::componentCount(format) == 4 && vk::componentBits(format, 0) == 8);
    constexpr u32 bitdepth = 4;
    vk::DeviceSize imageSize = width * height * bitdepth;

    auto [stagingBuffer, stagingBufferMemory] = engine->createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    m_mips = 1;
    if (samplerInfo.useMipmaps) {
        m_mips = static_cast<u32>(std::floor(std::log2(std::max(width, height)))) + 1;
    }

    std::tie(m_image, m_imageMemory) = engine->createImage(
        width, height,
        format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vk::SampleCountFlagBits::e1,
        m_mips
    );

    // transition image layout for optimal copying
    const auto commandBuffer = engine->beginSingleCommand();
    engine->transitionImageLayout(commandBuffer, m_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, m_mips);
    // copy staging buffer data to image
    vk::BufferImageCopy copyRegion = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    commandBuffer.copyBufferToImage(stagingBuffer, m_image, vk::ImageLayout::eTransferDstOptimal, copyRegion);
    engine->endSingleCommand(commandBuffer);
    generateMipmaps(engine);
    m_imageView = engine->createImageView(*m_image, format, vk::ImageAspectFlagBits::eColor, m_mips);

    vk::SamplerCreateInfo samplerCreateInfo = {
        .magFilter = samplerInfo.magFilter,
        .minFilter = samplerInfo.minFilter,
        .mipmapMode = samplerInfo.mipmapMode,
        .addressModeU = samplerInfo.wrapU,
        .addressModeV = samplerInfo.wrapV,
        .addressModeW = samplerInfo.wrapW,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = engine->getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = vk::LodClampNone,
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

void Texture::generateMipmaps(const VulkanEngine* engine) {
    const auto commandBuffer = engine->beginSingleCommand();

    vk::ImageMemoryBarrier barrier = {
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = m_image,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    u32 mipWidth = m_width;
    u32 mipHeight = m_height;

    for (u32 i = 1; i < m_mips; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;

        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead,
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal,
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal,

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, barrier);
        const std::array srcOffsets = {vk::Offset3D(0, 0, 0), vk::Offset3D(mipWidth, mipHeight, 1)};
        const std::array dstOffsets = {vk::Offset3D(0, 0, 0), vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight> 1 ? mipHeight / 2: 1, 1)};
        const vk::ImageBlit blit = {
            .srcSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = i - 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .srcOffsets = srcOffsets,
            .dstSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .dstOffsets = dstOffsets
        };

        commandBuffer.blitImage(m_image, vk::ImageLayout::eTransferSrcOptimal, m_image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, barrier);

        if (mipWidth > 1)
            mipWidth /= 2;
        if (mipHeight >  1)
            mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = m_mips - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, barrier);

    engine->endSingleCommand(commandBuffer);
}
