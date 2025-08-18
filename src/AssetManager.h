#pragma once

#include <tiny_gltf.h>
#include <glm/glm.hpp>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Common.h"

class VulkanEngine;

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> descriptions;
        // position
        descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sfloat,
            .offset = offsetof(Vertex, pos)
        };
        // color
        descriptions[1] = {
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = offsetof(Vertex, uv),
        };
        return descriptions;
    }
};
class Mesh {
public:
    Mesh(VulkanEngine* engine, const std::vector<glm::vec3>& positions, const std::vector<glm::vec2>& uvs, const std::vector<u16>& indexes);
    Mesh(VulkanEngine* engine, const std::vector<Vertex>& vertices, const std::vector<u16> &indexes);
    Mesh(Mesh&&) = default;
    void draw(const vk::raii::CommandBuffer &commandBuffer) const;
private:
    void createVertexBuffer(const std::vector<Vertex> &vertices);
    void createIndexBuffer(const std::vector<u16>& indexes);

    VulkanEngine* m_engine = nullptr;

    vk::raii::Buffer m_vertexBuffer = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory = nullptr;

    vk::raii::Buffer m_indexBuffer = nullptr;
    vk::raii::DeviceMemory m_indexBufferMemory = nullptr;

    u32 m_indexCount;
};

class AssetManager {
public:
    AssetManager(VulkanEngine* engine);
    void load(const char* root);
    std::vector<Mesh>& getMeshes();
private:
    void loadGLB(std::string path);

    static tinygltf::Buffer& getDataFromAccessor(tinygltf::Model ctx, u32 accessor);

    VulkanEngine* m_engine;

    tinygltf::TinyGLTF m_loader;

    std::vector<Mesh> m_meshes;
};
