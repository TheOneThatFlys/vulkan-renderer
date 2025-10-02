#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "ECS.h"
#include "LightSystem.h"
#include "Pipeline.h"
#include "UniformBufferBlock.h"
#include "BoundingVolumeRenderer.h"
#include "ModelSelector.h"
#include "Skybox.h"

struct FrameUniforms {
    glm::mat4 view;
    glm::mat4 projection;
};

struct ModelUniforms {
    glm::mat4 transform;
    glm::mat4 normal;
};

struct FragFrameData {
    glm::vec3 cameraPosition;
    std::array<LightSystem::PointLightFragData, LightSystem::MAX_LIGHTS> lights;
    u32 nLights;
    float far, fog;
};

struct RendererDebugInfo {
    u32 totalInstanceCount = 0;
    u32 renderedInstanceCount = 0;
    u32 materialSwitches = 0;
};

class Renderer3D final : public ECS::System {
public:
    explicit Renderer3D(vk::Extent2D extent);
    void render(const vk::raii::CommandBuffer &commandBuffer, const vk::Image &image, const vk::ImageView &imageView);

    void onEntityAdd(ECS::Entity entity) override;
    void onEntityRemove(ECS::Entity entity) override;

    const Pipeline* getPipeline() const;

    void rebuild();

    void setExtent(vk::Extent2D extent);
    RendererDebugInfo getDebugInfo() const;
    BoundingVolumeRenderer* getBoundingVolumeRenderer() const;
    ModelSelector* getModelSelector() const;
    const std::vector<ECS::Entity>& getLastRenderedEntities() const;
    ECS::Entity getCamera() const;

    void setSampleCount(vk::SampleCountFlagBits samples);
    vk::SampleCountFlagBits getSampleCount() const;

    void setSkybox(const std::shared_ptr<Skybox>& skybox);

    void highlightEntity(ECS::Entity entity);
    ECS::Entity getHighlightedEntity() const;

private:
    void createPipelines();
    void createAttachments();

    void beginRender(const vk::raii::CommandBuffer &commandBuffer, const vk::Image &image, const vk::ImageView &imageView) const;
    void setDynamicParameters(const vk::raii::CommandBuffer &commandBuffer) const;
    void setFrameUniforms(const vk::raii::CommandBuffer &commandBuffer);
    void drawModels(const vk::raii::CommandBuffer &commandBuffer);
    void drawSkybox(const vk::raii::CommandBuffer &commandBuffer);
    void endRender(const vk::raii::CommandBuffer &commandBuffer, const vk::Image &image) const;

    vk::Extent2D m_extent;

    std::unique_ptr<Pipeline> m_pipeline = nullptr;
    std::unique_ptr<Pipeline> m_xrayPipeline = nullptr;
    std::unique_ptr<Pipeline> m_skyboxPipeline = nullptr;

    std::unique_ptr<Image> m_colorImage;
    std::unique_ptr<Image> m_depthImage;

    vk::SampleCountFlagBits m_samples = vk::SampleCountFlagBits::e4;

    ECS::Entity m_camera;

    std::shared_ptr<Skybox> m_skybox;
    vk::raii::DescriptorSet m_skyboxDescriptor = nullptr;

    vk::raii::DescriptorSet m_frameDescriptor = nullptr;
    vk::raii::DescriptorSet m_modelDescriptor = nullptr;

    UniformBufferBlock<FrameUniforms> m_frameUniforms;
    DynamicUniformBufferBlock<ModelUniforms> m_modelUniforms;
    UniformBufferBlock<FragFrameData> m_fragFrameUniforms;

    std::unique_ptr<BoundingVolumeRenderer> m_boundingVolumeRenderer;
    std::unique_ptr<ModelSelector> m_modelSelector;

    RendererDebugInfo m_debugInfo;

    std::unordered_map<Material*, std::vector<ECS::Entity>> m_sortedEntities;
    std::vector<ECS::Entity> m_renderedEntities;

    ECS::Entity m_highlightedEntity = -1;
};
