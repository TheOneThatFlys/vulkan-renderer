#pragma once

#include <unordered_map>

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "ECS.h"

constexpr u32 FRAME_AVERAGE_COUNT = 32;

enum DebugFlags {
    eNormalizeRotation,
    eDisplayMatrix,
    eDisplayBoundingVolume
};

class SceneGraphDisplaySystem : public ECS::System {
public:
    void draw() const;
private:
    static void drawNodeRecursive(ECS::Entity entity);
    static void drawMatrix(glm::mat4& matrix);
};

class VulkanEngine;
class DebugWindow {
public:
    DebugWindow(VulkanEngine* engine, GLFWwindow *window, const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::Device& device, const vk::raii::Queue& queue);
    ~DebugWindow();
    void draw(const vk::raii::CommandBuffer& commandBuffer);

private:
    using UpdateCallback = void(*)(DebugWindow*);
    void setTimedUpdate(UpdateCallback func, int nFrames);

    void createUpdateCallbacks();

    void drawNodeRecursive(ECS::Entity entity);

    static void drawMatrix(const glm::mat4& mat);

    VulkanEngine* m_engine;
    SceneGraphDisplaySystem* m_graphDisplay;

    std::unordered_map<UpdateCallback, int> m_updateCallbacks;
    std::unordered_map<UpdateCallback, int> m_callbackLives;

    bool m_firstFrame = true;

    std::array<FrameTimeInfo, FRAME_AVERAGE_COUNT> m_frameTimes;
    u32 m_framePointer = 0;
    bool m_framesFilled = false;

    VRAMUsageInfo m_vramUsage;

    std::array<std::bitset<32>, ECS::MAX_ENTITIES> m_debugFlags;
};
