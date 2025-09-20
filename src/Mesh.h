#pragma once

#include <concepts>

#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "Vertex.h"
#include "Volumes.h"

class VulkanEngine;

template<typename V>
concept ValidVertex = requires (V v) {
    { v.pos } -> std::same_as<glm::vec3&>;
};

template<ValidVertex V = Vertex>
class Mesh {
public:
    Mesh(VulkanEngine* engine, const std::vector<V>& vertices, const std::vector<u32> &indexes);
    void draw(const vk::raii::CommandBuffer &commandBuffer) const;

    OBB getLocalOBB() const;
private:
    void createVertexBuffer(const std::vector<V> &vertices);

    void createIndexBuffer(const std::vector<u32>& indexes);

    // calculate the distance of the furthest vertex from the origin
    OBB resolveOBB(const std::vector<V>& vertices);

    VulkanEngine* m_engine = nullptr;

    vk::raii::Buffer m_vertexBuffer = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory = nullptr;

    vk::raii::Buffer m_indexBuffer = nullptr;
    vk::raii::DeviceMemory m_indexBufferMemory = nullptr;

    u32 m_indexCount;
    vk::IndexType m_indexType;

    OBB m_localOBB;
};
