#pragma once

#include <imgui.h>
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

class VulkanEngine;
class DebugWindow {
public:
    DebugWindow();
    ~DebugWindow();
    void draw(const vk::raii::CommandBuffer& commandBuffer);
    void rebuild() const;

private:
    void initVulkanImpl() const;
    using UpdateCallback = void(*)(DebugWindow*);
    void setTimedUpdate(UpdateCallback func, int nFrames);
    void createUpdateCallbacks();
    void drawNodeRecursive(ECS::Entity entity);
    void setFlagForAllEntities(DebugFlags flag, bool value);
    static void drawMatrix(const glm::mat4& mat);

    void performanceTab() const;
    void renderTab() const;
    void ecsTab();
    void searchTab();

    std::unordered_map<UpdateCallback, int> m_updateCallbacks;
    std::unordered_map<UpdateCallback, int> m_callbackLives;

    bool m_firstFrame = true;

    std::array<FrameTimeInfo, FRAME_AVERAGE_COUNT> m_frameTimes;
    u32 m_framePointer = 0;
    bool m_framesFilled = false;

    VRAMUsageInfo m_vramUsage;

    std::array<std::bitset<32>, ECS::MAX_ENTITIES> m_debugFlags;

    bool m_shouldFocusSearch = false;
    std::string m_searchText;
    ECS::Entity m_searchID = 0;
    static constexpr u32 m_searchCountLimit = 16;
    bool m_searchUseIDs = false;
};
