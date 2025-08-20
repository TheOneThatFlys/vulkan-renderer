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
    template<typename T>
    void createIndexBuffer(const std::vector<T>& indexes);

    static void validateAccessor(const tinygltf::Accessor& accessor, u32 componentType, u32 type);

    VulkanEngine* m_engine = nullptr;

    vk::raii::Buffer m_vertexBuffer = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory = nullptr;

    vk::raii::Buffer m_indexBuffer = nullptr;
    vk::raii::DeviceMemory m_indexBufferMemory = nullptr;

    u32 m_indexCount;
    vk::IndexType m_indexType;
};

class Texture {
public:
    Texture(const VulkanEngine *engine, const unsigned char* pixels, u32 width, u32 height, vk::Format format = vk::Format::eR8G8B8A8Srgb);

    const vk::raii::Image& getImage() const;
    const vk::raii::DeviceMemory& getImageMemory() const;
    const vk::raii::ImageView& getImageView() const;
    const vk::raii::Sampler& getSampler() const;
private:
    vk::raii::Image m_image = nullptr;
    vk::raii::DeviceMemory m_imageMemory = nullptr;
    vk::raii::ImageView m_imageView = nullptr;
    vk::raii::Sampler m_sampler = nullptr;
};

class Material {
public:
    Material(const Texture* base) : m_base(base) {}
    void use(const vk::raii::CommandBuffer& commandBuffer);

private:
    const Texture* m_base;
};

struct Model {
    const Mesh* mesh;
    const Material* material;
    glm::mat4 transform = glm::mat4(1.0f);
};

class AssetManager {
public:
    explicit AssetManager(VulkanEngine* engine);
    void load(const char* root);
    const std::vector<std::unique_ptr<Model>>& getModels() const;

    const Texture* getTexture(const std::string& key) const;
    const Material* getMaterial(const std::string& key) const;
    const Mesh* getMesh(const std::string& key) const;
private:
    void loadGLB(const std::string& path);
    void loadImage(std::string path);

    static std::string generateLocalKey(std::string path, u32 id);

    VulkanEngine* m_engine;

    tinygltf::TinyGLTF m_loader;

    std::unordered_map<std::string, std::unique_ptr<Mesh>> m_meshes;
    std::unordered_map<std::string, std::unique_ptr<Material>> m_materials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
    std::vector<std::unique_ptr<Model>> m_models;
};
