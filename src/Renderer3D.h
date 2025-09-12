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

struct RendererDebugInfo {
    u32 totalInstanceCount = 0;
    u32 renderedInstanceCount = 0;
};

class BoundingVolumeRenderer {
public:
    struct BoundingVolumeUniform {
        glm::mat4 transform;
        glm::vec3 colour;
    };

    explicit BoundingVolumeRenderer(VulkanEngine* engine);

    void draw(const vk::raii::CommandBuffer &commandBuffer);

    void queueSphere(const Sphere& sphere, const glm::vec3& colour);

private:
    struct ColouredSphere {
        Sphere sphere;
        glm::vec3 colour;
    };

    void createVolumes();

    VulkanEngine* m_engine;

    std::unique_ptr<Pipeline> m_pipeline = nullptr;

    vk::raii::DescriptorSet m_frameDescriptor = nullptr;
    vk::raii::DescriptorSet m_modelDescriptor = nullptr;

    UniformBufferBlock<FrameUniforms> m_frameUniforms;
    DynamicUniformBufferBlock<BoundingVolumeUniform> m_modelUniforms;

    std::vector<ColouredSphere> m_sphereQueue;

    std::unique_ptr<Mesh<BasicVertex>> m_sphereMesh = nullptr;
};

class Renderer3D final : public ECS::System {
public:
    explicit Renderer3D(VulkanEngine *engine, vk::Extent2D extent);
    void render(const vk::raii::CommandBuffer &commandBuffer, const vk::Image &image, const vk::ImageView &imageView);

    const Pipeline* getPipeline() const;

    void setExtent(vk::Extent2D extent);
    RendererDebugInfo getDebugInfo() const;
    BoundingVolumeRenderer& getBoundingVolumeRenderer();

    Sphere createBoundingVolume(ECS::Entity entity) const;

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
    std::unique_ptr<Pipeline> m_linePipeline = nullptr;

    vk::raii::Image m_depthBuffer = nullptr;
    vk::raii::DeviceMemory m_depthBufferMemory = nullptr;
    vk::raii::ImageView m_depthBufferImage = nullptr;

    ECS::Entity m_camera;

    vk::raii::DescriptorSet m_frameDescriptor = nullptr;
    vk::raii::DescriptorSet m_modelDescriptor = nullptr;

    UniformBufferBlock<FrameUniforms> m_frameUniforms;
    DynamicUniformBufferBlock<ModelUniforms> m_modelUniforms;
    UniformBufferBlock<FragFrameData> m_fragFrameUniforms;

    BoundingVolumeRenderer m_boundingVolumeRenderer;

    RendererDebugInfo m_debugInfo;
};
