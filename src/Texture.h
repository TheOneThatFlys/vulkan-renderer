#pragma once

#include <vulkan/vulkan_raii.hpp>
#include "Common.h"
class VulkanEngine;

struct SamplerInfo {
    vk::Filter minFilter = vk::Filter::eNearest;
    vk::Filter magFilter = vk::Filter::eNearest;
    vk::SamplerAddressMode wrapU = vk::SamplerAddressMode::eRepeat;
    vk::SamplerAddressMode wrapV = vk::SamplerAddressMode::eRepeat;
    vk::SamplerAddressMode wrapW = vk::SamplerAddressMode::eRepeat;
};

class Texture {
public:
    Texture(
        const VulkanEngine *engine,
        const unsigned char* pixels,
        u32 width, u32 height,
        vk::Format format = vk::Format::eR8G8B8A8Srgb,
        const SamplerInfo& samplerInfo = {}
    );

    const vk::raii::Image& getImage() const;
    const vk::raii::DeviceMemory& getImageMemory() const;
    const vk::raii::ImageView& getImageView() const;
    const vk::raii::Sampler& getSampler() const;
private:
    vk::raii::Image m_image = nullptr;
    vk::raii::DeviceMemory m_imageMemory = nullptr;
    vk::raii::ImageView m_imageView = nullptr;
    vk::raii::Sampler m_sampler = nullptr;
};