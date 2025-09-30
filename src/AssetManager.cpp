#include "AssetManager.h"

#include <filesystem>
#include <stack>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include "Components.h"

static std::unordered_map<u32, vk::SamplerAddressMode> g_wrapModeMap = {
    {TINYGLTF_TEXTURE_WRAP_REPEAT, vk::SamplerAddressMode::eRepeat},
    {TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE, vk::SamplerAddressMode::eClampToEdge},
    {TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT, vk::SamplerAddressMode::eMirroredRepeat}
};

AssetManager::AssetManager(VulkanEngine* engine) : m_engine(engine) {
    stbi_set_flip_vertically_on_load(true);

    std::array<u8, 4> whitePixels = {255, 255, 255, 255};

    m_textures.push_back(std::make_unique<Texture>(engine, whitePixels.data(), 1, 1));
    m_pureWhite1x1Texture = m_textures.back().get();

    std::array<u8, 4> normalPixels = {128, 128, 255, 0};
    m_textures.push_back(std::make_unique<Texture>(engine, normalPixels.data(), 1, 1, vk::Format::eR8G8B8A8Unorm));
    m_normal1x1Texture = m_textures.back().get();

    {
        std::vector<Vertex> vertices = {
            {{-1.0f,  1.0f, -1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{-1.0f,  1.0f, -1.0f}},

            {{-1.0f, -1.0f,  1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
            {{-1.0f,  1.0f, -1.0f}},
            {{-1.0f,  1.0f, -1.0f}},
            {{-1.0f,  1.0f,  1.0f}},
            {{-1.0f, -1.0f,  1.0f}},

            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},

            {{-1.0f, -1.0f,  1.0f}},
            {{-1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f, -1.0f,  1.0f}},
            {{-1.0f, -1.0f,  1.0f}},

            {{-1.0f,  1.0f, -1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{-1.0f,  1.0f,  1.0f}},
            {{-1.0f,  1.0f, -1.0f}},

            {{-1.0f, -1.0f, -1.0f}},
            {{-1.0f, -1.0f,  1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{-1.0f, -1.0f,  1.0f}},
            {{ 1.0f, -1.0f,  1.0}},
        };

        std::vector<u32> indexes;
        for (u32 i = 0; i < vertices.size(); ++i) {
            indexes.push_back(i);
        }
        m_meshes.push_back(std::make_unique<Mesh<>>(engine, vertices, indexes));
        m_unitCubeMesh = m_meshes.back().get();
    }
}

// Load glb file and return the root entity
ECS::Entity AssetManager::loadGLB(const std::string& path) {
    auto startTime = std::chrono::high_resolution_clock::now();

    tinygltf::Model ctx;
    std::string error, warning;
    m_loader.LoadBinaryFromFile(&ctx, &error, &warning, path);
    if (!error.empty())
        Logger::error("Error loading '{}': {}", path, error);
    if (!warning.empty())
        Logger::warn("Warning loading '{}': {}", path, warning);

    auto loadTime = std::chrono::high_resolution_clock::now();

    // temp pointers to resolve arrays
    std::vector<Texture*> textures;
    std::vector<Material*> materials;
    std::vector<Mesh<>*> meshes;
    textures.reserve(ctx.textures.size());
    materials.reserve(ctx.materials.size());
    meshes.reserve(ctx.meshes.size());
    // reserve space in actual arrays
    m_textures.reserve(m_textures.size() + textures.size());
    m_materials.reserve(m_materials.size() + materials.size());
    m_meshes.reserve(m_meshes.size() + meshes.size());

    // materials
    if (ctx.materials.empty())
        Logger::warn("Loaded file contains no materials");
    for (const auto & material : ctx.materials) {
        auto resolveTexture = [&](const i32 index, Texture* fallback, vk::Format format) -> Texture* {
            if (index == -1) {
                return fallback;
            }
            const tinygltf::Texture& texture = ctx.textures[index];
            const tinygltf::Sampler& sampler = ctx.samplers[texture.sampler];
            const tinygltf::Image& source = ctx.images[texture.source];

            SamplerInfo samplerInfo;
            samplerInfo.wrapU = g_wrapModeMap.at(sampler.wrapS);
            samplerInfo.wrapV = g_wrapModeMap.at(sampler.wrapT);

            auto resolveFilter = [&](const i32 filterType) -> vk::Filter {
                if (filterType == -1) return vk::Filter::eNearest;
                if (filterType == TINYGLTF_TEXTURE_FILTER_LINEAR || filterType == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST || filterType == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR) {
                    return vk::Filter::eLinear;
                }
                if (filterType == TINYGLTF_TEXTURE_FILTER_NEAREST || filterType == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST || filterType == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR) {
                    return vk::Filter::eNearest;
                }
                Logger::warn("Unrecognised filter type: {}", filterType);
                return vk::Filter::eNearest;
            };
            samplerInfo.minFilter = resolveFilter(sampler.minFilter);
            samplerInfo.magFilter = resolveFilter(sampler.magFilter);

            if (sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR || sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR) {
                samplerInfo.useMipmaps = true;
                samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
            }
            else if (sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST || sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST) {
                samplerInfo.useMipmaps = true;
                samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
            }

            m_textures.push_back(std::make_unique<Texture>(m_engine, source.image.data(), source.width, source.height, format, samplerInfo));
            return m_textures.back().get();
        };

        Texture *baseTexture = resolveTexture(material.pbrMetallicRoughness.baseColorTexture.index, m_pureWhite1x1Texture, vk::Format::eR8G8B8A8Srgb);
        Texture *metallicRoughnessTexture = resolveTexture(material.pbrMetallicRoughness.metallicRoughnessTexture.index, m_pureWhite1x1Texture, vk::Format::eR8G8B8A8Unorm);
        Texture *aoTexture = resolveTexture(material.occlusionTexture.index, m_pureWhite1x1Texture, vk::Format::eR8G8B8A8Unorm);
        Texture *normalTexture = resolveTexture(material.normalTexture.index, m_normal1x1Texture, vk::Format::eR8G8B8A8Unorm);
        m_materials.push_back(std::make_unique<Material>(m_engine, baseTexture, metallicRoughnessTexture, aoTexture, normalTexture));
        materials.emplace_back(m_materials.back().get());
    }
    // meshes
    if (ctx.meshes.empty())
        Logger::warn("Loaded file contains no meshes");
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

    ECS::Entity root;
    if (scene.nodes.size() == 1) {
        root = -1;
    }
    else {
        root = ECS::createEntity();
        HierarchyComponent::addEmpty(root);
        ECS::addComponent<NamedComponent>(root, {path});
        ECS::addComponent<Transform>(root, {});
    }
    std::stack<std::tuple<i32, ECS::Entity>> nodesToVisit;
    for (i32 nodeID : scene.nodes) {
        nodesToVisit.emplace(nodeID, root);
    }

    while (!nodesToVisit.empty()) {
        auto [nodeID, parent] = nodesToVisit.top();
        nodesToVisit.pop();
        const tinygltf::Node& node = ctx.nodes[nodeID];

        // create entity
        ECS::Entity entity = ECS::createEntity();
        if (root == -1) root = entity;

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
        ECS::addComponent<HierarchyComponent>(entity, {parent, {}});
        ECS::addComponent<NamedComponent>(entity, {node.name});
        if (parent != -1) {
            ECS::getComponent<HierarchyComponent>(parent).children.push_back(entity);
        }
        for (i32 child : node.children) {
            nodesToVisit.emplace(child, entity);
        }
    }

    Transform::updateTransform(root);

    auto endTime = std::chrono::high_resolution_clock::now();

    Logger::info("Loaded '{}' in {} ms [read = {} ms]", path, std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime).count(), std::chrono::duration_cast<std::chrono::milliseconds>(loadTime-startTime).count());

    return root;
}

std::unique_ptr<Skybox> AssetManager::loadSkybox(const std::string &folderPath, const char* ext) {
    static constexpr std::array names = { "px", "nx", "py", "ny", "pz", "nz" };
    std::array<unsigned char*, 6> data = {};
    i32 width, height, channels;
    for (u32 i = 0; i < 6; ++i) {
        data[i] = stbi_load((folderPath + "/" + names[i] + "." + ext).c_str(), &width, &height, &channels, STBI_rgb_alpha);
        stbi__vertical_flip(data[i], width, height, 4); // probably not supposed to use this but for some reason stbi load on flip refuses to work
        if (data[i] == nullptr) throw std::runtime_error(std::format("Unable to load image at {} ({})", folderPath, stbi_failure_reason()));
    }
    auto skybox = std::make_unique<Skybox>(m_engine, data, width, height);
    for (u32 i = 0; i < 6; ++i) {
        stbi_image_free(data[i]);
    }
    return std::move(skybox);
}

Mesh<> * AssetManager::getUnitCube() const {
    return m_unitCubeMesh;
}

std::unique_ptr<Mesh<>> AssetManager::loadMesh(const tinygltf::Model& ctx, const tinygltf::Mesh &mesh) {
    if (mesh.primitives.size() > 1) {
        Logger::warn(std::format("Mesh loader does not currently support more than one 1 primitive per mesh, only the first was loaded ({} total)", mesh.primitives.size()));
    }
    const tinygltf::Primitive& primitive = mesh.primitives.at(0);

    const auto loadBuffer = [&](u32 accessorIndex, u32 componentType, u32 type) -> const unsigned char* {
        const tinygltf::Accessor& accessor = ctx.accessors[accessorIndex];
        const tinygltf::BufferView& bufferView = ctx.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = ctx.buffers[bufferView.buffer];
        validateAccessor(accessor, componentType, type);
        return &buffer.data[accessor.byteOffset + bufferView.byteOffset];
    };

    bool hasUVs = primitive.attributes.contains("TEXCOORD_0");
    if (!hasUVs) Logger::warn("{} does not contain TEXCOORD_0 attribute", mesh.name);

    // load vertex info
    if (!primitive.attributes.contains("POSITION")) Logger::error("{} does not contain POSITION attribute", mesh.name);
    if (!primitive.attributes.contains("NORMAL")) Logger::error("{} does not contain NORMAL attribute", mesh.name);
    if (!primitive.attributes.contains("TANGENT")) Logger::error("{} does not contain TANGENT attribute", mesh.name);

    const unsigned char* positions = loadBuffer(primitive.attributes.at("POSITION"), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3);
    const unsigned char* uvs = hasUVs ? loadBuffer(primitive.attributes.at("TEXCOORD_0"), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC2) : nullptr;
    const unsigned char* normals = loadBuffer(primitive.attributes.at("NORMAL"), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3);
    const unsigned char* tangents = loadBuffer(primitive.attributes.at("TANGENT"), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC4);

    constexpr float nullUVs[] = {0.0f, 0.0f};
    std::vector<Vertex> vertices;
    const u64 numberOfVertices = ctx.accessors[primitive.attributes.at("POSITION")].count;
    vertices.reserve(numberOfVertices);
    for (u64 i = 0; i < numberOfVertices; ++i) {
        const auto pos = reinterpret_cast<const float*>(&positions[i * 12]);
        const auto uv = hasUVs ? reinterpret_cast<const float*>(&uvs[i * 8]) : nullUVs;
        const auto normal = reinterpret_cast<const float*>(&normals[i * 12]);
        const auto tangent = reinterpret_cast<const float*>(&tangents[i * 16]);
        vertices.emplace_back(
            glm::vec3(pos[0], pos[1], pos[2]), // position
            glm::vec2(uv[0], uv[1]), // uv
            glm::vec3(normal[0], normal[1], normal[2]), // normal
            glm::vec3(tangent[0], tangent[1], tangent[2]) // tangent
        );
    }

    // load indexes
    const tinygltf::Accessor& indexAccessor = ctx.accessors[primitive.indices];
    const tinygltf::BufferView& indexBufferView = ctx.bufferViews[indexAccessor.bufferView];
    const tinygltf::Buffer& indexBuffer = ctx.buffers[indexBufferView.buffer];

    const size_t indexStart = indexAccessor.byteOffset + indexBufferView.byteOffset;
    std::vector<u32> indexes;
    indexes.reserve(indexAccessor.count);
    switch (indexAccessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            for (u32 i = 0; i < indexAccessor.count; ++i) {
                const auto index = *reinterpret_cast<const u32*>(&indexBuffer.data[indexStart + i * 4]);
                indexes.emplace_back(index);
            }
            break;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            for (u32 i = 0; i < indexAccessor.count; ++i) {
                const auto index = static_cast<u32>(*reinterpret_cast<const u16*>(&indexBuffer.data[indexStart + i * 2]));
                indexes.emplace_back(index);
            }
            break;

        default:
            Logger::error(std::format("Unknown index type: {}", indexAccessor.componentType));
    }
    return std::make_unique<Mesh<>>(m_engine, vertices, indexes);
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
