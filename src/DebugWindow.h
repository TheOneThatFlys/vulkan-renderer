#pragma once

#include <GLFW/glfw3.h>
#include <compare>
import vulkan_hpp;

class DebugWindow {
public:
    DebugWindow(GLFWwindow *window, vk::raii::Instance& instance, vk::raii::PhysicalDevice& physicalDevice, vk::raii::Device& device, vk::raii::Queue& queue, vk::raii::RenderPass& renderPass);
    ~DebugWindow();
    void draw(const vk::raii::CommandBuffer& commandBuffer);
};
