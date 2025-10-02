#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "Image.h"

class Skybox {
public:
    Skybox(const std::array<unsigned char*, 6>& pixels, u32 width, u32 height, vk::Format format = vk::Format::eR8G8B8A8Srgb);

    void addToSet(const vk::raii::DescriptorSet& set, u32 binding) const;

    const Image& getImage() const;
    const vk::raii::Sampler& getSampler() const;

private:
    std::unique_ptr<Image> m_image;
    vk::raii::Sampler m_sampler = nullptr;
};
