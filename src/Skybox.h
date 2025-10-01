#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "Common.h"

class Skybox {
public:
    Skybox(const std::array<unsigned char*, 6>& pixels, u32 width, u32 height, vk::Format format = vk::Format::eR8G8B8A8Srgb);

    void addToSet(const vk::raii::DescriptorSet& set, u32 binding) const;

    const vk::raii::Image& getImage() const;
    const vk::raii::ImageView& getImageView() const;
    const vk::raii::Sampler& getSampler() const;

private:
    vk::raii::Image m_image = nullptr;
    vk::raii::DeviceMemory m_imageMemory = nullptr;
    vk::raii::ImageView m_imageView = nullptr;
    vk::raii::Sampler m_sampler = nullptr;
};
