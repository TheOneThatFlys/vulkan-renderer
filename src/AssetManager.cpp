#include "AssetManager.h"

#include <filesystem>
#include <stack>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include "Components.h"
#include "ECS.h"
#include "VulkanEngine.h"

AssetManager::AssetManager(VulkanEngine* engine) : m_engine(engine) {
    stbi_set_flip_vertically_on_load(true);
}

void AssetManager::load(const char* root) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const std::filesystem::path& filepath = entry.path();

        std::string path = filepath.generic_string();
        std::string extension = filepath.extension().generic_string();

        // model
        if (extension == ".glb") {
            // Logger::info(std::format("Loading model at {}", path));
            // loadGLB(path);
        }
        // image
        else if (extension == ".jpg" || extension == ".png") {
            Logger::info(std::format("Loading texture at {}", path));
            loadImage(path);
        }
        else {
            Logger::info(std::format("Found asset at {} which is not supported", path));
        }
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    Logger::info(std::format("Loaded assets in {} ms", duration.count()));
}

void AssetManager::loadGLB(const std::string& path) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    tinygltf::Model ctx;
    std::string error, warning;
    m_loader.LoadBinaryFromFile(&ctx, &error, &warning, path);
    if (!error.empty())
        Logger::error(error);
    if (!warning.empty())
        Logger::warn(warning);

    // temp pointers to resolve arrays
    std::vector<Texture*> textures;
    std::vector<Material*> materials;
    std::vector<Mesh*> meshes;
    textures.reserve(ctx.textures.size());
    materials.reserve(ctx.materials.size());
    meshes.reserve(ctx.meshes.size());
    // reserve space in actual arrays
    m_textures.reserve(m_textures.size() + textures.size());
    m_materials.reserve(m_materials.size() + materials.size());
    m_meshes.reserve(m_meshes.size() + meshes.size());

    // textures
    for (const auto & texture : ctx.textures) {
        const tinygltf::Sampler& sampler = ctx.samplers[texture.sampler];
        const tinygltf::Image& source = ctx.images[texture.source];
        m_textures.push_back(std::make_unique<Texture>(m_engine, source.image.data(), source.width, source.height));
        textures.emplace_back(m_textures.back().get());
    }
    // materials
    for (const auto & material : ctx.materials) {
        m_materials.push_back(std::make_unique<Material>(m_engine, textures.at(material.pbrMetallicRoughness.baseColorTexture.index)));
        materials.emplace_back(m_materials.back().get());
    }
    // meshes
    for (const auto & mesh : ctx.meshes) {
        m_meshes.push_back(loadMesh(ctx, mesh));
        meshes.emplace_back(m_meshes.back().get());
    }
    // nodes
    if (ctx.scenes.size() > 1)
        Logger::warn("Loaded file contains more than one scene, only first one was loaded ({} total)", ctx.scenes.size());
    const tinygltf::Scene& scene = ctx.scenes.at(0);
    if (scene.nodes.empty())
        Logger::warn("Loaded scene contained no nodes");

    auto root = ECS::createEntity();
    ECS::addComponent<Transform>(root, {});
    ECS::addComponent<HierarchyComponent>(root, {0, {}, 0});
    ECS::addComponent<NamedComponent>(root, {path});
    std::stack<std::tuple<i32, u32, ECS::Entity>> nodesToVisit;
    for (i32 nodeID : ctx.scenes[0].nodes) {
        nodesToVisit.emplace(nodeID, 1, root);
    }
    u32 i = 0;
    while (!nodesToVisit.empty()) {
        ++i;
        auto [nodeID, level, parent] = nodesToVisit.top();
        nodesToVisit.pop();
        const tinygltf::Node& node = ctx.nodes[nodeID];

        // create entity
        ECS::Entity entity = ECS::createEntity();
        // add model if it exists
        if (node.mesh != -1) {
            ECS::addComponent<Model3D>(entity, {meshes[node.mesh], materials[ctx.meshes[node.mesh].primitives[0].material]});
        }

        Transform transform;
        // transform components are already specified
        if (!node.translation.empty()) {
            transform.position = glm::vec3(static_cast<float>(node.translation[0]), static_cast<float>(node.translation[1]), static_cast<float>(node.translation[2]));
        }
        if (!node.rotation.empty()) {
            transform.rotation = glm::quat(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
        }
        if (!node.scale.empty()) {
            transform.scale = glm::vec3(static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]), static_cast<float>(node.scale[2]));
        }
        // transform components given as a matrix
        if (!node.matrix.empty()) {
            Logger::warn("Matrix transform specifiers are not supported yet");
        }

        ECS::addComponent<Transform>(entity, transform);
        ECS::addComponent<HierarchyComponent>(entity, {parent, {}, level});
        ECS::addComponent<NamedComponent>(entity, {node.name});
        ECS::getComponent<HierarchyComponent>(parent).children.push_back(entity);
        for (i32 child : node.children) {
            nodesToVisit.emplace(child, level + 1, entity);
        }
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    Logger::info(std::format("Loaded {} in {} ms", path, duration.count()));
}

std::unique_ptr<Mesh> AssetManager::loadMesh(const tinygltf::Model& ctx, const tinygltf::Mesh &mesh) {
    if (mesh.primitives.size() > 1) {
        Logger::warn(std::format("Mesh loader does not currently support more than one 1 primitive per mesh, only the first was loaded ({} total)", mesh.primitives.size()));
    }
    const tinygltf::Primitive& primitive = mesh.primitives.at(0);

    // load vertex info
    std::vector<Vertex> vertices;
    const tinygltf::Accessor& positionAccessor = ctx.accessors[primitive.attributes.at("POSITION")];
    const tinygltf::BufferView& positionBufferView = ctx.bufferViews[positionAccessor.bufferView];
    const tinygltf::Buffer& positionBuffer = ctx.buffers[positionBufferView.buffer];
    const tinygltf::Accessor& uvAccessor = ctx.accessors[primitive.attributes.at("TEXCOORD_0")];
    const tinygltf::BufferView& uvBufferView = ctx.bufferViews[uvAccessor.bufferView];
    const tinygltf::Buffer& uvBuffer = ctx.buffers[uvBufferView.buffer];

    validateAccessor(positionAccessor, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3);
    validateAccessor(uvAccessor, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC2);

    vertices.reserve(positionAccessor.count);
    const size_t positionStart = positionAccessor.byteOffset + positionBufferView.byteOffset;
    const size_t uvStart = uvAccessor.byteOffset + uvBufferView.byteOffset;
    for (u32 i = 0; i < positionAccessor.count; ++i) {
        const auto pos = reinterpret_cast<const float*>(&positionBuffer.data[positionStart + i * 12]);
        const auto uv = reinterpret_cast<const float*>(&uvBuffer.data[uvStart + i * 8]);
        vertices.emplace_back(glm::vec3(pos[0], pos[1], pos[2]), glm::vec2(uv[0], uv[1]));
    }

    // load indexes
    const tinygltf::Accessor& indexAccessor = ctx.accessors[primitive.indices];
    const tinygltf::BufferView& indexBufferView = ctx.bufferViews[indexAccessor.bufferView];
    const tinygltf::Buffer& indexBuffer = ctx.buffers[indexBufferView.buffer];

    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        std::vector<u32> indexes;
        size_t indexStart = indexAccessor.byteOffset + indexBufferView.byteOffset;
        indexes.reserve(indexAccessor.count);
        for (u32 i = 0; i < indexAccessor.count; ++i) {
            const auto index = *reinterpret_cast<const u32*>(&indexBuffer.data[indexStart + i * 4]);
            indexes.emplace_back(index);
        }

        return std::make_unique<Mesh>(m_engine, vertices, indexes);
    }

    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        std::vector<u16> indexes;
        size_t indexStart = indexAccessor.byteOffset + indexBufferView.byteOffset;
        indexes.reserve(indexAccessor.count);
        for (u32 i = 0; i < indexAccessor.count; ++i) {
            const auto index = *reinterpret_cast<const u16*>(&indexBuffer.data[indexStart + i * 2]);
            indexes.emplace_back(index);
        }

        return std::make_unique<Mesh>(m_engine, vertices, indexes);
    }

    Logger::error(std::format("Unknown index type: {}", indexAccessor.componentType));
    return nullptr;
}

void AssetManager::loadImage(std::string path) {
    i32 width, height, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) throw std::runtime_error(std::format("Unable to load image at {} ({})", path, stbi_failure_reason()));
    m_textures.emplace_back(std::make_unique<Texture>(m_engine, pixels, static_cast<u32>(width), static_cast<u32>(height)));
    stbi_image_free(pixels);
}

void AssetManager::validateAccessor(const tinygltf::Accessor &accessor, u32 componentType, u32 type) {
    if (accessor.componentType != componentType || accessor.type != type)
        Logger::error("Unexpected accessor format ({}, {}), expected ({}, {})", accessor.componentType, accessor.type, componentType, type);
}
