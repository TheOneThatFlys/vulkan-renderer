#pragma once

#include <unordered_map>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include "Common.h"

constexpr u32 FRAME_AVERAGE_COUNT = 32;

class VulkanEngine;
class DebugWindow {
public:
    DebugWindow(VulkanEngine* engine, GLFWwindow *window, const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::Device& device, const vk::raii::Queue& queue, const vk::raii::RenderPass& renderPass);
    ~DebugWindow();
    void draw(const vk::raii::CommandBuffer& commandBuffer);

private:
    using UpdateCallback = void(*)(DebugWindow*);
    void setTimedUpdate(UpdateCallback func, int nFrames);

    void createUpdateCallbacks();

    VulkanEngine* m_engine;

    std::unordered_map<UpdateCallback, int> m_updateCallbacks;
    std::unordered_map<UpdateCallback, int> m_callbackLives;

    bool m_firstFrame = true;

    std::array<FrameTimeInfo, FRAME_AVERAGE_COUNT> m_frameTimes;
    u32 m_framePointer = 0;
    bool m_framesFilled = false;

    VRAMUsageInfo m_vramUsage;
};
