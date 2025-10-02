#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "Common.h"

struct ImageCreateInfo {
    u32 width, height;
    u32 depth = 1;
    vk::Format format;
    vk::ImageUsageFlags usage;
    vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    vk::ImageType imageType = vk::ImageType::e2D;
    vk::ImageAspectFlagBits aspect = vk::ImageAspectFlagBits::eColor;
    u32 mips = 1;
    u32 arrayLayers = 1;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
    vk::ImageViewType viewType = vk::ImageViewType::e2D;
    vk::ImageCreateFlags flags = {};
};

struct ImageTransitionInfo {
    vk::ImageLayout oldLayout;
    vk::ImageLayout newLayout;
    u32 mips = 1;
    u32 arrayLayers = 1;
    vk::ImageAspectFlagBits aspect = vk::ImageAspectFlagBits::eColor;
};

class Image {
public:
    explicit Image(const ImageCreateInfo& info);
    void changeLayout(const vk::raii::CommandBuffer& commandBuffer, const ImageTransitionInfo& info) const;
    static void changeLayout(const vk::raii::CommandBuffer& commandBuffer, vk::Image image, const ImageTransitionInfo& info);

    const vk::raii::Image& getImage() const;
    const vk::raii::DeviceMemory& getMemory() const;
    const vk::raii::ImageView& getView() const;
private:
    vk::ImageAspectFlagBits m_aspect;
    vk::raii::Image m_image = nullptr;
    vk::raii::DeviceMemory m_imageMemory = nullptr;
    vk::raii::ImageView m_imageView = nullptr;
};
