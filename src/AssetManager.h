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
    Mesh(VulkanEngine* engine, const tinygltf::Model& ctx, const tinygltf::Primitive& primitive);
    Mesh(VulkanEngine* engine, const std::vector<Vertex>& vertices, const std::vector<u32> &indexes);
    Mesh(Mesh&&) = default;
    void draw(const vk::raii::CommandBuffer &commandBuffer) const;
private:
    void createVertexBuffer(const std::vector<Vertex> &vertices);
    void createIndexBuffer(const std::vector<u32>& indexes);

    static void validateAccessor(const tinygltf::Accessor& accessor, u32 componentType, u32 type);

    VulkanEngine* m_engine = nullptr;

    vk::raii::Buffer m_vertexBuffer = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory = nullptr;

    vk::raii::Buffer m_indexBuffer = nullptr;
    vk::raii::DeviceMemory m_indexBufferMemory = nullptr;

    u32 m_indexCount;
};

class Material {
public:
    Material();
    void use(const vk::raii::CommandBuffer& commandBuffer);
};

class Texture {
public:
    Texture();
private:
    vk::raii::Image m_image;
    vk::raii::DeviceMemory m_deviceMemory;
    vk::raii::ImageView m_imageView;
    vk::raii::Sampler m_sampler;
};

struct Model {
    Mesh* mesh;
    Material* material;
    glm::mat4 transform;
};

class AssetManager {
public:
    AssetManager(VulkanEngine* engine);
    void load(const char* root);
    std::vector<std::unique_ptr<Mesh>>& getMeshes();
private:
    void loadGLB(std::string path);

    VulkanEngine* m_engine;

    tinygltf::TinyGLTF m_loader;

    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<std::unique_ptr<Material>> m_materials;
    std::vector<std::unique_ptr<Model>> m_models;
};
