#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Volumes.h"
#include "Pipeline.h"
#include "UniformBufferBlock.h"

class VulkanEngine;
class BoundingVolumeRenderer {
public:
    explicit BoundingVolumeRenderer(VulkanEngine* engine, const Renderer3D* parentRenderer);

    void draw(const vk::raii::CommandBuffer &commandBuffer);
    void rebuild();

    void queueSphere(const Sphere& sphere, const glm::vec3& colour);
    void queueOBB(const OBB& obb, const glm::vec3& colour);

private:
    struct ColouredSphere {
        Sphere sphere;
        glm::vec3 colour;
    };

    struct ColouredOBB {
        OBB obb;
        glm::vec3 colour;
    };

    struct BoundingVolumeUniform {
        glm::mat4 transform;
        glm::vec3 colour;
    };

    struct BoundingFrameUniform {
        glm::mat4 view;
        glm::mat4 projection;
    };

    void createPipeline(vk::SampleCountFlagBits samples);
    void createVolumes();

    VulkanEngine* m_engine;

    std::unique_ptr<Pipeline> m_pipeline = nullptr;

    vk::raii::DescriptorSet m_frameDescriptor = nullptr;
    vk::raii::DescriptorSet m_modelDescriptor = nullptr;

    UniformBufferBlock<BoundingFrameUniform> m_frameUniforms;
    DynamicUniformBufferBlock<BoundingVolumeUniform> m_modelUniforms;

    std::vector<ColouredSphere> m_sphereQueue;
    std::vector<ColouredOBB> m_obbQueue;

    std::unique_ptr<Mesh<BasicVertex>> m_sphereMesh = nullptr;
    std::unique_ptr<Mesh<BasicVertex>> m_cubeMesh = nullptr;
};
