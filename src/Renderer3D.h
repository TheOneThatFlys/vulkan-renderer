#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "ECS.h"
#include "LightSystem.h"
#include "Pipeline.h"
#include "UniformBufferBlock.h"

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
};

class Renderer3D final : public ECS::System {
public:
    explicit Renderer3D(VulkanEngine *engine, vk::Extent2D extent);
    void render(const vk::raii::CommandBuffer &commandBuffer, const vk::Image &image, const vk::ImageView &imageView);

    const Pipeline* getPipeline() const;

    void setPolygonMode(vk::PolygonMode mode) { m_polygonMode = mode; }
    vk::PolygonMode getPolygonMode() const { return m_polygonMode; }
    void setExtent(vk::Extent2D extent);

private:
    void createPipelines();
    void createDepthBuffer();

    void beginRender(const vk::raii::CommandBuffer &commandBuffer, const vk::Image &image, const vk::ImageView &imageView) const;
    void setDynamicParameters(const vk::raii::CommandBuffer &commandBuffer) const;
    void setFrameUniforms(const vk::raii::CommandBuffer &commandBuffer);
    void drawModels(const vk::raii::CommandBuffer &commandBuffer);
    void endRender(const vk::raii::CommandBuffer &commandBuffer, const vk::Image &image) const;

    VulkanEngine* m_engine;
    vk::Extent2D m_extent;

    std::unique_ptr<Pipeline> m_pipeline = nullptr;

    vk::raii::Image m_depthBuffer = nullptr;
    vk::raii::DeviceMemory m_depthBufferMemory = nullptr;
    vk::raii::ImageView m_depthBufferImage = nullptr;

    ECS::Entity m_camera;

    vk::raii::DescriptorSet m_frameDescriptor = nullptr;
    vk::raii::DescriptorSet m_modelDescriptor = nullptr;

    UniformBufferBlock<FrameUniforms> m_frameUniforms;
    DynamicUniformBufferBlock<ModelUniforms> m_modelUniforms;
    UniformBufferBlock<FragFrameData> m_fragFrameUniforms;

    vk::PolygonMode m_polygonMode = vk::PolygonMode::eFill;
};
