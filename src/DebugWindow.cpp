#include "DebugWindow.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <iomanip>

#include "VulkanRenderer.h"

DebugWindow::DebugWindow(VulkanRenderer* vulkanRenderer, GLFWwindow *window, const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physicalDevice,
    const vk::raii::Device& device, const vk::raii::Queue& queue, const vk::raii::RenderPass& renderPass) {

    m_vulkanRenderer = vulkanRenderer;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo = {
        .ApiVersion = vk::ApiVersion14,
        .Instance = *instance,
        .PhysicalDevice = *physicalDevice,
        .Device = *device,
        .Queue = *queue,
        .RenderPass = *renderPass,
        .MinImageCount = 2,
        .ImageCount = 2,
        .DescriptorPoolSize = 64,
    };

    ImGui_ImplVulkan_Init(&initInfo);
}

DebugWindow::~DebugWindow() {
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplVulkan_Shutdown();
}

void DebugWindow::draw(const vk::raii::CommandBuffer& commandBuffer) {
    if (!m_firstFrame) {
        m_firstFrame = true;
        return;
    }

    // begin frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    m_frameTimes[m_framePointer++] = m_vulkanRenderer->getFrameTimeInfo();
    if (m_framePointer == FRAME_AVERAGE_COUNT) {
        m_framePointer = 0;
        if (!m_framesFilled) m_framesFilled = true;
    }
    // create elements
    ImGui::SetNextWindowPos(ImVec2(8, 8));
    ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTabBar("navbar")) {
        if (ImGui::BeginTabItem("Main")) {
            if (ImGui::TreeNode("Camera")) {

                ImGui::TreePop();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Performance")) {
            FrameTimeInfo avgTimes = { 1, 1 };
            std::string frameTimeText, drawTimeText = "Loading...";
            if (m_framesFilled) {
                for (auto const& frameTime : m_frameTimes) {
                    avgTimes.frameTime += frameTime.frameTime;
                    avgTimes.gpuTime += frameTime.gpuTime;
                }
                avgTimes.frameTime /= FRAME_AVERAGE_COUNT;
                avgTimes.gpuTime /= FRAME_AVERAGE_COUNT;
            }

            ImGui::Text(std::format("FPS:        {:.0f}", 1000 / avgTimes.frameTime).c_str());
            ImGui::Text(std::format("eFPS:       {:.0f}", 1000 / avgTimes.gpuTime).c_str());
            ImGui::Text(std::format("Frame time: {:.3f} ms", avgTimes.frameTime).c_str());
            ImGui::Text(std::format("Draw time:  {:.3f} ms", avgTimes.gpuTime).c_str());

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // render frame
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, *commandBuffer);

}
