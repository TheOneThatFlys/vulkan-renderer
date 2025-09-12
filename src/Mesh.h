#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "Vertex.h"

class VulkanEngine;
template<typename V = Vertex>
class Mesh {
public:
    Mesh(VulkanEngine* engine, const std::vector<V>& vertices, const std::vector<u32> &indexes);
    void draw(const vk::raii::CommandBuffer &commandBuffer) const;

    float getMaxDistance() const;
private:
    void createVertexBuffer(const std::vector<V> &vertices);

    void createIndexBuffer(const std::vector<u32>& indexes);

    // calculate the distance of the furthest vertex from the origin
    float resolveMaxDistance(const std::vector<V>& vertices);

    VulkanEngine* m_engine = nullptr;

    vk::raii::Buffer m_vertexBuffer = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory = nullptr;

    vk::raii::Buffer m_indexBuffer = nullptr;
    vk::raii::DeviceMemory m_indexBufferMemory = nullptr;

    u32 m_indexCount;
    vk::IndexType m_indexType;

    float m_maxDistance = 0.0f; // greatest distance from origin to a vertex`
};
