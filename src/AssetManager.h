#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include "Common.h"
#include "ECS.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"
#include "Skybox.h"

class AssetManager {
public:
    AssetManager();
    ECS::Entity loadGLB(const std::filesystem::path& path);
    std::unique_ptr<Skybox> loadSkybox(const std::string& folderPath, const char* ext = "png");

    Mesh<>* getUnitCube() const;
private:
    std::unique_ptr<Mesh<>> loadMesh(const fastgltf::Asset &ctx, const fastgltf::Mesh &mesh);
    void loadImage(std::string path);

    std::vector<std::unique_ptr<Mesh<>>> m_meshes;
    std::vector<std::unique_ptr<Material>> m_materials;
    std::vector<std::unique_ptr<Texture>> m_textures;

    Texture* m_pureWhite1x1Texture;
    Texture* m_normal1x1Texture;
    Mesh<>* m_unitCubeMesh;
};
