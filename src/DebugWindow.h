#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include "Common.h"

constexpr u32 FRAME_AVERAGE_COUNT = 32;

class VulkanRenderer;
class DebugWindow {
public:
    DebugWindow(VulkanRenderer* vulkanRenderer, GLFWwindow *window, const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::Device& device, const vk::raii::Queue& queue, const vk::raii::RenderPass& renderPass);
    ~DebugWindow();
    void draw(const vk::raii::CommandBuffer& commandBuffer);

private:
    VulkanRenderer* m_vulkanRenderer;

    bool m_firstFrame = false;

    std::array<FrameTimeInfo, FRAME_AVERAGE_COUNT> m_frameTimes;
    u32 m_framePointer = 0;
    bool m_framesFilled = false;
};
