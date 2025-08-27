#pragma once

#include <tiny_gltf.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "ECS.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"

class VulkanEngine;

class AssetManager {
public:
    explicit AssetManager(VulkanEngine* engine);
    void load(const char* root);
    void loadGLB(const std::string& path);
private:
    std::unique_ptr<Mesh> loadMesh(const tinygltf::Model &ctx, const tinygltf::Mesh &mesh);
    void loadImage(std::string path);

    static void validateAccessor(const tinygltf::Accessor& accessor, u32 componentType, u32 type);

    VulkanEngine* m_engine;

    tinygltf::TinyGLTF m_loader;

    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<std::unique_ptr<Material>> m_materials;
    std::vector<std::unique_ptr<Texture>> m_textures;

    Texture* m_pureWhite1x1Texture;
    Texture* m_normal1x1Texture;
};
