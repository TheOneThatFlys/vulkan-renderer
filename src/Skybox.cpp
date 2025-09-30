#include "Skybox.h"

#include <vulkan/vulkan_format_traits.hpp>

#include "Renderer3D.h"
#include "VulkanEngine.h"

Skybox::Skybox(VulkanEngine *engine, const std::array<unsigned char *, 6> &pixels, const u32 width, const u32 height, const vk::Format format) : m_engine(engine) {
    assert(vk::componentCount(format) == 4 && vk::componentBits(format, 0) == 8);
    constexpr u32 bitdepth = 4;
    vk::DeviceSize imageSize = width * height * bitdepth;

    auto [stagingBuffer, stagingBufferMemory] = engine->createBuffer(imageSize * 6, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    void* data = stagingBufferMemory.mapMemory(0, imageSize * 6);
    for (u32 i = 0; i < 6; ++i) {
        memcpy(static_cast<unsigned char*>(data) + i * imageSize, pixels[i], imageSize);
    }
    stagingBufferMemory.unmapMemory();

    std::tie(m_image, m_imageMemory) = engine->createImage({
        .width = width,
        .height = height,
        .format = format,
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .arrayLayers = 6,
        .flags = vk::ImageCreateFlagBits::eCubeCompatible
    });

    const auto commandBuffer = engine->beginSingleCommand();

    engine->transitionImageLayout(commandBuffer, {
        .image = m_image,
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

    commandBuffer.copyBufferToImage(stagingBuffer, m_image, vk::ImageLayout::eTransferDstOptimal, copyRegion);

    engine->transitionImageLayout(commandBuffer, {
        .image = m_image,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .arrayLayers = 6
    });
    engine->endSingleCommand(commandBuffer);

    vk::ImageViewCreateInfo createInfo = {
        .image = m_image,
        .viewType = vk::ImageViewType::eCube,
        .format = format,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 6
        }
    };

    m_imageView = vk::raii::ImageView(engine->getDevice(), createInfo);

    vk::SamplerCreateInfo samplerCreateInfo = {
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
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

void Skybox::addToSet(const vk::raii::DescriptorSet &set, const u32 binding) const {
    const vk::DescriptorImageInfo imageInfo = {
        .sampler = m_sampler,
        .imageView = m_imageView,
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
    m_engine->getDevice().updateDescriptorSets(writeInfo, nullptr);
}

const vk::raii::Image & Skybox::getImage() const {
    return m_image;
}

const vk::raii::ImageView & Skybox::getImageView() const {
    return m_imageView;
}

const vk::raii::Sampler & Skybox::getSampler() const {
    return m_sampler;
}
