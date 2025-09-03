#include "DebugWindow.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <ranges>

#include "Components.h"
#include "Renderer3D.h"
#include "VulkanEngine.h"

void SceneGraphDisplaySystem::draw() const {
    ImGui::Text("Total entities: %u", m_entities.size());
    for (const ECS::Entity entity : m_entities) {
        const auto hierarchy = ECS::getComponentOptional<HierarchyComponent>(entity);
        if (!hierarchy || hierarchy->parent == -1) {
            drawNodeRecursive(entity);
        }
    }
}

void SceneGraphDisplaySystem::drawNodeRecursive(ECS::Entity entity) {
    std::string name;
    if (ECS::hasComponent<NamedComponent>(entity)) {
        name = ECS::getComponent<NamedComponent>(entity).name;
    }
    else {
        name = std::format("Entity #{}", entity);
    }

    if (ImGui::TreeNodeEx(name.c_str())) {

        // Components sorted alphabetically
        ImGui::SeparatorText("Components");
        // Meta
        if (ImGui::TreeNode("Meta")) {
            ImGui::Text("ID: %d", entity);
            ImGui::Text(std::format("Signature: {}", ECS::getSignature(entity).to_string()).c_str());
            ImGui::TreePop();
        }

        // Camera
        if (ECS::hasComponent<ControlledCamera>(entity)) {
            if (ImGui::TreeNode("Camera")) {
                auto& camera = ECS::getComponent<ControlledCamera>(entity);
                ImGui::DragFloat3("Position", glm::value_ptr(camera.position), 0.1f);
                float yawPitchTemp[] = {camera.yaw, camera.pitch};
                if (ImGui::DragFloat2("Yaw/Pitch", yawPitchTemp, 0.02f, glm::radians(-180.0f), glm::radians(180.0f), "%.3f", ImGuiSliderFlags_WrapAround)) {
                    camera.yaw = yawPitchTemp[0];
                    camera.pitch = std::ranges::clamp(yawPitchTemp[1], glm::radians(-89.9f), glm::radians(89.9f));
                }
                ImGui::SliderAngle("FOV", &camera.fov, 0.0001f, 180.0f);
                ImGui::SliderFloat("Speed", &camera.speed, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
                ImGui::SliderFloat("Sensitivity", &camera.sensitivity, 0.0001f, 0.01f, "%.4f", ImGuiSliderFlags_NoRoundToFormat);

                ImGui::TreePop();
            }
        }

        // Hierarchy
        if (ECS::hasComponent<HierarchyComponent>(entity)) {
            if (ImGui::TreeNode("Hierarchy")) {
                auto& hierarchy = ECS::getComponent<HierarchyComponent>(entity);
                ImGui::Text("Parent: %d", hierarchy.parent);
                std::string childrenString;
                for (auto child : hierarchy.children) {
                    childrenString += std::to_string(child) + ", ";
                }
                if (!childrenString.empty())
                    childrenString = childrenString.substr(0, childrenString.size() - 2);
                ImGui::Text(std::format("Children: [{}]", childrenString).c_str());
                ImGui::TreePop();
            }
        }
        // Light
        if (ECS::hasComponent<PointLight>(entity)) {
            if (ImGui::TreeNode("Light")) {
                auto& light = ECS::getComponent<PointLight>(entity);
                ImGui::ColorEdit3("Colour", glm::value_ptr(light.colour));
                ImGui::DragFloat("Strength", &light.strength, 1, 0, std::numeric_limits<float>::max());
                ImGui::TreePop();
            }
        }
        // Model
        if (ECS::hasComponent<Model3D>(entity)) {
            if (ImGui::TreeNode("Model3D")) {
                const auto& model = ECS::getComponent<Model3D>(entity);
                ImGui::Text("Mesh: <0x%X>", model.mesh);
                ImGui::Text("Material: <0x%X>", model.material);
                ImGui::TreePop();
            }
        }
        // Transform
        if (ECS::hasComponent<Transform>(entity)) {
            if (ImGui::TreeNode("Transform")) {
                static bool showMatrix = false;
                auto&[position, rotation, scale, transform] = ECS::getComponent<Transform>(entity);
                ImGui::DragFloat3("Position", glm::value_ptr(position), 0.01f);
                ImGui::DragFloat4("Rotation", glm::value_ptr(rotation), 0.01f, -1.0f, 1.0f);
                ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f, 0.0f, std::numeric_limits<float>::max());

                static bool normalizeRotation = false;
                ImGui::Checkbox("Normalize rotation", &normalizeRotation);
                if (normalizeRotation)
                    rotation = glm::normalize(rotation);

                ImGui::SameLine();
                ImGui::Checkbox("Show matrix", &showMatrix);

                if (showMatrix)
                    drawMatrix(transform);

                ImGui::TreePop();
            }
        }

        if (ECS::hasComponent<HierarchyComponent>(entity)) {
            auto& hierarchy = ECS::getComponent<HierarchyComponent>(entity);
            if (!hierarchy.children.empty()) {
                ImGui::SeparatorText("Children");
                for (const ECS::Entity child : hierarchy.children) {
                    drawNodeRecursive(child);
                }
            }
        }

        ImGui::TreePop();
    }
}

void SceneGraphDisplaySystem::drawMatrix(glm::mat4 &matrix) {
    for (u32 row = 0; row < 4; ++row) {
        for (u32 col = 0; col < 4; ++col) {
            ImGui::Text("% .3f", matrix[col][row]);
            if (col != 3) ImGui::SameLine();
        }
    }
}

DebugWindow::DebugWindow(VulkanEngine* engine, GLFWwindow *window, const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::Device& device, const vk::raii::Queue& queue) {
    m_engine = engine;
    m_graphDisplay = ECS::registerSystem<SceneGraphDisplaySystem>();
    ECS::setSystemSignature<SceneGraphDisplaySystem>(ECS::createSignature<>()); // views all entities in the ECS

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    auto colourFormat = static_cast<VkFormat>(m_engine->getSwapColourFormat());
    ImGui_ImplVulkan_InitInfo initInfo = {
        .ApiVersion = vk::ApiVersion14,
        .Instance = *instance,
        .PhysicalDevice = *physicalDevice,
        .Device = *device,
        .Queue = *queue,
        .MinImageCount = 2,
        .ImageCount = 2,
        .DescriptorPoolSize = 64,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colourFormat,
            .depthAttachmentFormat = static_cast<VkFormat>(m_engine->getDepthFormat()),
        }
    };

    ImGui_ImplVulkan_Init(&initInfo);

    createUpdateCallbacks();
}

DebugWindow::~DebugWindow() {
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplVulkan_Shutdown();
}

void DebugWindow::draw(const vk::raii::CommandBuffer& commandBuffer) {
    if (m_firstFrame) {
        m_firstFrame = false;
        for (const auto &func: m_updateCallbacks | std::views::keys) {
            func(this);
        }
        return;
    }

    // begin frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    m_frameTimes[m_framePointer++] = m_engine->getFrameTimeInfo();
    if (m_framePointer == FRAME_AVERAGE_COUNT) {
        m_framePointer = 0;
        if (!m_framesFilled) m_framesFilled = true;
    }
    // create elements
    ImGui::SetNextWindowPos(ImVec2(8, 8));
    ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTabBar("navbar")) {
        if (ImGui::BeginTabItem("Performance")) {
            // frame times
            FrameTimeInfo avgTimes = { 1, 1, 1 };
            std::string frameTimeText, drawTimeText = "Loading...";
            if (m_framesFilled) {
                for (auto const& frameTime : m_frameTimes) {
                    avgTimes.frameTime += frameTime.frameTime;
                    avgTimes.gpuTime += frameTime.gpuTime;
                    avgTimes.cpuTime += frameTime.cpuTime;
                }
                avgTimes.frameTime /= FRAME_AVERAGE_COUNT;
                avgTimes.gpuTime /= FRAME_AVERAGE_COUNT;
                avgTimes.cpuTime /= FRAME_AVERAGE_COUNT;
            }

            ImGui::Text(std::format("Frame:        {:.3f} ms ({:.0f} fps)", avgTimes.frameTime, 1000 / avgTimes.frameTime).c_str());
            ImGui::Text(std::format("Draw:         {:.3f} ms ({:.0f} fps)", avgTimes.gpuTime, 1000 / avgTimes.gpuTime).c_str());
            ImGui::Text(std::format("CPU (update): {:.3f} ms", avgTimes.cpuTime).c_str());

            ImGui::Separator();
            // memory usage
            ImGui::Text("VRAM Usage");
            size_t usedGB = m_vramUsage.gpuUsed;
            size_t totalGB = m_vramUsage.gpuAvailable;
            std::string progress = std::format("{} / {}", storageSizeToString(usedGB), storageSizeToString(totalGB));
            ImGui::ProgressBar(static_cast<float>(usedGB) / static_cast<float>(totalGB), ImVec2(0.0f, 0.0f), progress.c_str());

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Render")) {
            ImGui::SeparatorText("Swapchain");

            using Resolution = std::pair<u32, u32>;

            constexpr std::array resolutions = {
                Resolution{2560, 1440},
                Resolution{1920, 1080},
                Resolution{1280, 720},
            };
            const auto resToString = [&resolutions](const Resolution& res) -> std::string {
                if (std::ranges::find(resolutions, res) == resolutions.end()) {
                    return std::format("Custom ({}x{})", res.first, res.second);
                }
                return std::format("{}x{}", res.first, res.second);
            };

            const Resolution actualResolution = m_engine->getWindowSize();
            if (ImGui::BeginCombo("Resolution", resToString(actualResolution).c_str())) {
                for (const auto& resolution : resolutions) {
                    const bool isSelected = resolution == actualResolution;
                    if (ImGui::Selectable(resToString(resolution).c_str(), isSelected)) {
                        m_engine->setWindowSize(resolution.first, resolution.second);
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            bool isVsync = m_engine->getPresentMode() == vk::PresentModeKHR::eFifo;
            if (ImGui::Checkbox("VSync", &isVsync)) {
                m_engine->setPresentMode(isVsync ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate);
            }

            ImGui::SeparatorText("Rasterizer");
            bool isWireframe = m_engine->getRenderer()->getPolygonMode() == vk::PolygonMode::eLine;
            if (ImGui::Checkbox("Wireframes", &isWireframe))
                m_engine->getRenderer()->setPolygonMode(isWireframe ? vk::PolygonMode::eLine : vk::PolygonMode::eFill);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("ECS")) {
            m_graphDisplay->draw();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();

    // render frame
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, *commandBuffer);

    // update callbacks
    for (auto& [func, time] : m_updateCallbacks) {
        m_callbackLives[func] -= 1;
        if (m_callbackLives[func] == 0) {
            m_callbackLives[func] = time;
            func(this);
        }
    }
}

void DebugWindow::setTimedUpdate(UpdateCallback func, int nFrames) {
    m_updateCallbacks[func] = nFrames;
    m_callbackLives[func] = nFrames;
}

void DebugWindow::createUpdateCallbacks() {
    setTimedUpdate([](DebugWindow* window) -> void {
        window->m_vramUsage = window->m_engine->getVramUsage();
    }, 60);
}
