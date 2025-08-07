#include "DebugWindow.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

DebugWindow::DebugWindow(GLFWwindow *window, vk::raii::Instance& instance, vk::raii::PhysicalDevice& physicalDevice,
    vk::raii::Device& device, vk::raii::Queue& queue, vk::raii::RenderPass& renderPass) {

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();

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
    // begin frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // create elements
    ImGui::ShowDemoWindow();

    // render frame
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, *commandBuffer);

}
