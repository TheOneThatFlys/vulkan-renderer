#include "DebugWindow.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <ranges>

#include "Components.h"
#include "EntitySystem.h"
#include "Renderer3D.h"
#include "VulkanEngine.h"

DebugWindow::DebugWindow(VulkanEngine* engine, GLFWwindow *window) : m_engine(engine) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;

    ImGui_ImplGlfw_InitForVulkan(window, true);
    initVulkanImpl();
    createUpdateCallbacks();

    m_searchText.resize(64);
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
    std::vector<UpdateCallback> deferedActions;

    // begin frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    m_frameTimes[m_framePointer++] = m_engine->getFrameTimeInfo();
    if (m_framePointer == FRAME_AVERAGE_COUNT) {
        m_framePointer = 0;
        if (!m_framesFilled) m_framesFilled = true;
    }

    const auto [windowWidth, windowHeight] = m_engine->getWindowSize();
    ImGui::SetNextWindowSizeConstraints({-1.0f, -1.0f}, {-1.0f, static_cast<float>(windowHeight) - 16.0f});
    ImGui::SetNextWindowPos(ImVec2(8, 8));
    ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTabBar("navbar")) {
        performanceTab();
        renderTab();
        ecsTab();
        searchTab();

        ImGui::EndTabBar();
    }
    ImGui::End();

    // render frame
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, *commandBuffer);

    // render bounding volumes
    const auto renderer = m_engine->getRenderer();
    BoundingVolumeRenderer* boundingVolumeRenderer = renderer->getBoundingVolumeRenderer();
    for (const auto entity : renderer->getLastRenderedEntities()) {
        if (!m_debugFlags.at(entity).test(eDisplayBoundingVolume)) continue;
        if (entity == renderer->getModelSelector()->getSelected()) continue;
        boundingVolumeRenderer->queueOBB(ECS::getComponent<BoundingVolume>(entity).obb, glm::vec3(1.0f, 1.0f, 1.0f));
    }

    // update callbacks
    for (auto& [func, time] : m_updateCallbacks) {
        m_callbackLives[func] -= 1;
        if (m_callbackLives[func] == 0) {
            m_callbackLives[func] = time;
            func(this);
        }
    }
}

void DebugWindow::rebuild() const {
    ImGui_ImplVulkan_Shutdown();
    initVulkanImpl();
}

void DebugWindow::initVulkanImpl() const{
    auto colourFormat = static_cast<VkFormat>(m_engine->getSwapColourFormat());
    ImGui_ImplVulkan_InitInfo initInfo = {
        .ApiVersion = vk::ApiVersion14,
        .Instance = *m_engine->getInstance(),
        .PhysicalDevice = *m_engine->getPhysicalDevice(),
        .Device = *m_engine->getDevice(),
        .Queue = *m_engine->getGraphicsQueue(),
        .MinImageCount = 2,
        .ImageCount = 2,
        .MSAASamples = static_cast<VkSampleCountFlagBits>(m_engine->getRenderer()->getSampleCount()),
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

void DebugWindow::drawNodeRecursive(ECS::Entity entity) {
    std::string name;
    if (ECS::hasComponent<NamedComponent>(entity)) {
        name = ECS::getComponent<NamedComponent>(entity).name;
    }
    else {
        name = std::format("Entity #{}", entity);
    }

    static constexpr auto defaultTreeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;

    if (ImGui::TreeNodeEx(name.c_str(), defaultTreeFlags)) {

        // Components sorted alphabetically
        ImGui::SeparatorText("Components");
        // Meta
        if (ImGui::TreeNodeEx("Meta", defaultTreeFlags)) {
            ImGui::Text("ID: %d", entity);
            ImGui::Text(std::format("Signature: {}", ECS::getSignature(entity).to_string()).c_str());
            ImGui::TreePop();
        }

        // Bounding Volume
        if (ECS::hasComponent<BoundingVolume>(entity)) {
            if (ImGui::TreeNodeEx("Bounding Volume", defaultTreeFlags)) {
                auto& boundingVolume = ECS::getComponent<BoundingVolume>(entity);

                bool showBoundingVolume = m_debugFlags.at(entity).test(eDisplayBoundingVolume);
                ImGui::Checkbox("Show", &showBoundingVolume);
                m_debugFlags.at(entity).set(eDisplayBoundingVolume, showBoundingVolume);

                glm::vec3 t1 = boundingVolume.obb.center;
                ImGui::DragFloat3("Center", glm::value_ptr(t1), 0, 0, 0, "%.3f", ImGuiSliderFlags_NoInput);

                glm::vec3 t2 = boundingVolume.obb.extent;
                ImGui::DragFloat3("Extent", glm::value_ptr(t2), 0, 0, 0, "%.3f", ImGuiSliderFlags_NoInput);

                float t3[] = {boundingVolume.obb.rotation.x, boundingVolume.obb.rotation.y, boundingVolume.obb.rotation.z, boundingVolume.obb.rotation.w};
                ImGui::DragFloat4("Rotation", t3, 0, 0, 0, "%.3f", ImGuiSliderFlags_NoInput);

                ImGui::TreePop();
            }
        }
        // Camera
        if (ECS::hasComponent<ControlledCamera>(entity)) {
            if (ImGui::TreeNodeEx("Camera", defaultTreeFlags)) {
                auto& camera = ECS::getComponent<ControlledCamera>(entity);
                ImGui::DragFloat3("Position", glm::value_ptr(camera.position), 0.1f);
                float yawPitchTemp[] = {camera.yaw, camera.pitch};
                if (ImGui::DragFloat2("Yaw/Pitch", yawPitchTemp, 0.02f, glm::radians(-180.0f), glm::radians(180.0f), "%.3f", ImGuiSliderFlags_WrapAround)) {
                    camera.yaw = yawPitchTemp[0];
                    camera.pitch = std::ranges::clamp(yawPitchTemp[1], glm::radians(-89.9f), glm::radians(89.9f));
                }
                ImGui::SliderAngle("FOV", &camera.fov, 0.0f, 180.0f);
                ImGui::SliderFloat("Speed", &camera.speed, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
                ImGui::SliderFloat("Sensitivity", &camera.sensitivity, 0.0001f, 0.01f, "%.4f", ImGuiSliderFlags_NoRoundToFormat);

                ImGui::DragFloat("Near", &camera.near, 0.1f, 0.0f, FLT_MAX);
                ImGui::DragFloat("Far", &camera.far, 0.1f, 0.0f, FLT_MAX);


                ImGui::TreePop();
            }
        }
        // Hierarchy
        if (ECS::hasComponent<HierarchyComponent>(entity)) {
            if (ImGui::TreeNodeEx("Hierarchy", ImGuiTreeNodeFlags_SpanAvailWidth)) {
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
            if (ImGui::TreeNodeEx("Light", defaultTreeFlags)) {
                auto& light = ECS::getComponent<PointLight>(entity);
                ImGui::ColorEdit3("Colour", glm::value_ptr(light.colour));
                ImGui::DragFloat("Strength", &light.strength, 1, 0, std::numeric_limits<float>::max());
                ImGui::TreePop();
            }
        }
        // Model
        if (ECS::hasComponent<Model3D>(entity)) {
            if (ImGui::TreeNodeEx("Model3D", defaultTreeFlags)) {
                const auto& model = ECS::getComponent<Model3D>(entity);
                ImGui::Text("Mesh: <0x%X>", model.mesh);
                ImGui::Text("Material: <0x%X>", model.material);

                if (ImGui::Button("Highlight in world")) {
                    m_engine->getRenderer()->highlightEntity(entity);
                }

                ImGui::TreePop();
            }
        }
        // Transform
        if (ECS::hasComponent<Transform>(entity)) {
            if (ImGui::TreeNodeEx("Transform", defaultTreeFlags)) {
                auto&[position, rotation, scale, transform] = ECS::getComponent<Transform>(entity);
                bool changed = false;
                changed |= ImGui::DragFloat3("Position", glm::value_ptr(position), 0.01f);
                changed |= ImGui::DragFloat4("Rotation", glm::value_ptr(rotation), 0.01f, -1.0f, 1.0f);
                changed |= ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f, 0.0f, std::numeric_limits<float>::max());

                if (changed)
                    Transform::updateTransform(entity);

                bool normalizeRotation = m_debugFlags.at(entity).test(eNormalizeRotation);
                ImGui::Checkbox("Normalize rotation", &normalizeRotation);
                m_debugFlags.at(entity).set(eNormalizeRotation, normalizeRotation);
                if (normalizeRotation) {
                    rotation = glm::normalize(rotation);
                    Transform::updateTransform(entity);
                }

                ImGui::SameLine();

                bool showMatrix = m_debugFlags.at(entity).test(eDisplayMatrix);
                ImGui::Checkbox("Show matrix", &showMatrix);
                m_debugFlags.at(entity).set(eDisplayMatrix, showMatrix);
                if (showMatrix) {
                    ImGui::Separator();
                    drawMatrix(transform);
                }

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

void DebugWindow::setFlagForAllEntities(const DebugFlags flag, const bool value) {
    for (const auto entity : ECS::getSystem<EntitySystem>()->get()) {
        m_debugFlags.at(entity).set(flag, value);
    }
}

void DebugWindow::drawMatrix(const glm::mat4 &matrix) {
    auto transposed = glm::transpose(matrix);
    ImGui::PushItemWidth(-FLT_MIN);
    for (i32 i = 0; i < 4; ++i) {
        auto row = glm::vec4(transposed[i]);
        ImGui::PushID(i);
        ImGui::DragFloat4("", glm::value_ptr(row), 0, 0, 0, "%.3f", ImGuiSliderFlags_NoInput);
        ImGui::PopID();
    }
    ImGui::PopItemWidth();
}

void DebugWindow::performanceTab() const {
    if (ImGui::BeginTabItem("Performance")) {
        // frame times
        FrameTimeInfo avgTimes = { 0.0, 0.0, 0.0, 0.0 };
        std::string frameTimeText, drawTimeText = "Loading...";
        if (m_framesFilled) {
            for (auto const& frameTime : m_frameTimes) {
                avgTimes.frameTime += frameTime.frameTime;
                avgTimes.gpuTime += frameTime.gpuTime;
                avgTimes.cpuTime += frameTime.cpuTime;
                avgTimes.drawWriteTime += frameTime.drawWriteTime;
            }
            avgTimes.frameTime /= FRAME_AVERAGE_COUNT;
            avgTimes.gpuTime /= FRAME_AVERAGE_COUNT;
            avgTimes.cpuTime /= FRAME_AVERAGE_COUNT;
            avgTimes.drawWriteTime /= FRAME_AVERAGE_COUNT;
        }

        ImGui::Text(std::format("Frame:        {:.3f} ms ({:.0f} fps)", avgTimes.frameTime, 1000 / avgTimes.frameTime).c_str());
        ImGui::Text(std::format("Draw:         {:.3f} ms ({:.0f} fps)", avgTimes.gpuTime, 1000 / avgTimes.gpuTime).c_str());
        ImGui::Text(std::format("CPU (update): {:.3f} ms", avgTimes.cpuTime).c_str());
        ImGui::Text(std::format("Cmd-write:    {:.3f} ms", avgTimes.drawWriteTime).c_str());

        ImGui::Separator();
        // memory usage
        ImGui::Text("VRAM Usage");
        size_t usedGB = m_vramUsage.gpuUsed;
        size_t totalGB = m_vramUsage.gpuAvailable;
        std::string progress = std::format("{} / {}", storageSizeToString(usedGB), storageSizeToString(totalGB));
        ImGui::ProgressBar(static_cast<float>(usedGB) / static_cast<float>(totalGB), ImVec2(0.0f, 0.0f), progress.c_str());

        ImGui::Separator();

        const auto rendererInfo = m_engine->getRenderer()->getDebugInfo();
        ImGui::Text(std::format("Total Instances:    {}", rendererInfo.totalInstanceCount).c_str());
        ImGui::Text(std::format("Rendered Instances: {}", rendererInfo.renderedInstanceCount).c_str());
        ImGui::Text(std::format("Material Switches:  {}", rendererInfo.materialSwitches).c_str());

        ImGui::EndTabItem();
    }
}

void DebugWindow::renderTab() const {
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

        static constexpr std::array sampleOptions = { vk::SampleCountFlagBits::e1, vk::SampleCountFlagBits::e2, vk::SampleCountFlagBits::e4, vk::SampleCountFlagBits::e8 };
        static constexpr std::array sampleNames = { "Off", "MSAAx2", "MSAAx4", "MSAAx8" };

        const auto currentSamples = m_engine->getRenderer()->getSampleCount();
        int i = 0;
        switch (currentSamples) {
            case vk::SampleCountFlagBits::e1:
                i = 0;
                break;
            case vk::SampleCountFlagBits::e2:
                i = 1;
                break;
            case vk::SampleCountFlagBits::e4:
                i = 2;
                break;
            case vk::SampleCountFlagBits::e8:
                i = 3;
                break;
            default:
                i = -1;
        }
        if (ImGui::SliderInt("Antialiasing", &i, 0, static_cast<int>(sampleOptions.size()) - 1, sampleNames[i])) {
            m_engine->getRenderer()->setSampleCount(sampleOptions.at(i));
        }

        bool isVsync = m_engine->getPresentMode() == vk::PresentModeKHR::eFifo;
        if (ImGui::Checkbox("VSync", &isVsync)) {
            m_engine->setPresentMode(isVsync ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate);
        }

        ImGui::EndTabItem();
    }
}

void DebugWindow::ecsTab() {
    if (ImGui::BeginTabItem("ECS")) {
        const auto& entities = ECS::getSystem<EntitySystem>()->get();

        ImGui::SeparatorText("Bounding volumes");

        if (ImGui::Button("Show all")) {
            setFlagForAllEntities(eDisplayBoundingVolume, true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Hide all")) {
            setFlagForAllEntities(eDisplayBoundingVolume, false);
        }

        ImGui::SeparatorText("Highlighting");
        const auto renderer = m_engine->getRenderer();
        int s = renderer->getHighlightedEntity();
        ImGui::InputInt("Current ID", &s);
        renderer->highlightEntity(s);
        if (ImGui::Button("Clear")) {
            renderer->highlightEntity(ECS::NULL_ENTITY);
        }
        ImGui::SameLine();
        if (ImGui::Button("Jump to")) {
            m_searchID = renderer->getHighlightedEntity();
            m_searchUseIDs = true;
            m_shouldFocusSearch = true;
        }

        ImGui::SeparatorText("Scene");

        ImGui::Text("Total entities: %u", entities.size());
        for (const ECS::Entity entity : entities) {
            const auto hierarchy = ECS::getComponentOptional<HierarchyComponent>(entity);
            if (!hierarchy || hierarchy->parent == -1) {
                drawNodeRecursive(entity);
            }
        }

        ImGui::EndTabItem();
    }
}

void DebugWindow::searchTab() {
    ImGuiTabItemFlags flags = 0;
    if (m_shouldFocusSearch) {
        flags |= ImGuiTabItemFlags_SetSelected;
    }
    if (ImGui::BeginTabItem("Search", nullptr, flags)) {
        ImGui::Checkbox("ID Search", &m_searchUseIDs);
        ImGui::PushID("SearchBar");
        ImGui::PushItemWidth(-FLT_MIN); // remove label spacing
        if (m_searchUseIDs) {
            ImGui::InputInt("", &m_searchID);
        }
        else {
            ImGui::InputTextWithHint("", "Enter name", m_searchText.data(), m_searchText.size());
        }
        ImGui::PopID();
        ImGui::PopItemWidth();

        ImGui::Separator();

        const auto& entitySystem = ECS::getSystem<EntitySystem>();
        if (m_searchUseIDs) {
            if (entitySystem->contains(m_searchID)) {
                if (m_shouldFocusSearch) {
                    m_shouldFocusSearch = false;
                    ImGui::SetNextItemOpen(true);
                }
                drawNodeRecursive(m_searchID);
            }
            else {
                ImGui::Text("No results :(");
                m_shouldFocusSearch = false;
            }
        }
        else {
            u32 count = 0;
            for (const ECS::Entity entity : entitySystem->getEntities()) {
                if (ECS::hasComponent<NamedComponent>(entity)) {
                    std::string entityName = ECS::getComponent<NamedComponent>(entity).name;
                    if (tolower(ECS::getComponent<NamedComponent>(entity).name).find(tolower(m_searchText).c_str()) != std::string::npos) {
                        drawNodeRecursive(entity);
                        count++;
                        if (count >= m_searchCountLimit) {
                            ImGui::Text("...");
                            break;
                        }
                    }
                }
            }
            if (count == 0) {
                ImGui::Text("No results :(");
            }
        }


        ImGui::EndTabItem();
    }
}
