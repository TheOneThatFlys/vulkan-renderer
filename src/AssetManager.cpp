#include "AssetManager.h"

#include <filesystem>
#include <stack>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/glm_element_traits.hpp>

#include "Components.h"

AssetManager::AssetManager() {
    stbi_set_flip_vertically_on_load(true);

    std::array<u8, 4> whitePixels = {255, 255, 255, 255};

    m_textures.push_back(std::make_unique<Texture>(whitePixels.data(), 1, 1));
    m_pureWhite1x1Texture = m_textures.back().get();

    std::array<u8, 4> normalPixels = {128, 128, 255, 0};
    m_textures.push_back(std::make_unique<Texture>(normalPixels.data(), 1, 1, vk::Format::eR8G8B8A8Unorm));
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
        m_meshes.push_back(std::make_unique<Mesh<>>(vertices, indexes));
        m_unitCubeMesh = m_meshes.back().get();
    }
}

// Load glb file and return the root entity
ECS::Entity AssetManager::loadGLB(const std::filesystem::path& path) {
    auto startTime = std::chrono::high_resolution_clock::now();

    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        Logger::error("Error creating buffer for '{}': {}", path.string(), fastgltf::getErrorName(data.error()));
    }
    auto ctx = parser.loadGltfBinary(data.get(), path.parent_path().string());
    if (ctx.error() != fastgltf::Error::None) {
        Logger::error("Error loading '{}': {}", path.string(), fastgltf::getErrorName(ctx.error()));
    }
    if (auto error = fastgltf::validate(ctx.get()); error != fastgltf::Error::None) {
        Logger::error("Error with loaded GLB '{}': {}", path.string(), fastgltf::getErrorName(error));
    }

    auto loadTime = std::chrono::high_resolution_clock::now();

    // temp pointers to resolve arrays
    std::vector<Texture*> textures;
    std::vector<Material*> materials;
    std::vector<Mesh<>*> meshes;
    textures.reserve(ctx->textures.size());
    materials.reserve(ctx->materials.size());
    meshes.reserve(ctx->meshes.size());
    // reserve space in actual arrays
    m_textures.reserve(m_textures.size() + textures.size());
    m_materials.reserve(m_materials.size() + materials.size());
    m_meshes.reserve(m_meshes.size() + meshes.size());

    // materials
    if (ctx->materials.empty())
        Logger::warn("Loaded file contains no materials");
    for (const auto & material : ctx->materials) {
        auto resolveTexture = [&](const size_t index, vk::Format format) -> Texture* {
            const fastgltf::Texture& texture = ctx->textures[index];
            const fastgltf::Sampler& sampler = ctx->samplers[texture.samplerIndex.value()];
            const fastgltf::Image& source = ctx->images[texture.imageIndex.value()];

            auto resolveWrap = [&](const std::optional<fastgltf::Wrap> wrap) -> vk::SamplerAddressMode {
                if (!wrap.has_value()) return vk::SamplerAddressMode::eRepeat;
                switch (wrap.value()) {
                    case fastgltf::Wrap::ClampToEdge: return vk::SamplerAddressMode::eClampToEdge;
                    case fastgltf::Wrap::Repeat: return vk::SamplerAddressMode::eRepeat;
                    case fastgltf::Wrap::MirroredRepeat: return vk::SamplerAddressMode::eMirroredRepeat;
                    default: return vk::SamplerAddressMode::eRepeat;
                }
            };

            SamplerInfo samplerInfo;
            samplerInfo.wrapU = resolveWrap(sampler.wrapS);
            samplerInfo.wrapV = resolveWrap(sampler.wrapT);

            auto resolveFilter = [&](const std::optional<fastgltf::Filter> filter) -> vk::Filter {
                if (!filter.has_value()) return vk::Filter::eNearest;
                if (filter == fastgltf::Filter::Linear || filter == fastgltf::Filter::LinearMipMapNearest || filter == fastgltf::Filter::LinearMipMapLinear) {
                    return vk::Filter::eLinear;
                }
                if (filter == fastgltf::Filter::Nearest || filter == fastgltf::Filter::NearestMipMapLinear || filter == fastgltf::Filter::NearestMipMapNearest) {
                    return vk::Filter::eNearest;
                }
                Logger::warn("Unrecognised filter type");
                return vk::Filter::eNearest;
            };
            samplerInfo.minFilter = resolveFilter(sampler.minFilter);
            samplerInfo.magFilter = resolveFilter(sampler.magFilter);

            if (sampler.minFilter == fastgltf::Filter::NearestMipMapLinear || sampler.minFilter == fastgltf::Filter::LinearMipMapLinear) {
                samplerInfo.useMipmaps = true;
                samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
            }
            else if (sampler.minFilter == fastgltf::Filter::NearestMipMapNearest || sampler.minFilter == fastgltf::Filter::LinearMipMapNearest) {
                samplerInfo.useMipmaps = true;
                samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
            }

            if (!std::holds_alternative<fastgltf::sources::BufferView>(source.data))
                Logger::error("Unsupported image storage mode. Images must be embedded in a .glb file.");

            auto& view = std::get<fastgltf::sources::BufferView>(source.data);
            auto& bufferView = ctx->bufferViews[view.bufferViewIndex];
            auto& buffer = ctx->buffers[bufferView.bufferIndex];

            if (!std::holds_alternative<fastgltf::sources::Array>(buffer.data))
                Logger::error("Buffer load corrupt. Was LoadExternalBuffers specified?");
            auto& vector = std::get<fastgltf::sources::Array>(buffer.data).bytes;

            i32 loadedWidth, loadedHeight, loadedChannels;
            const u8* imageBytes = stbi_load_from_memory(reinterpret_cast<u8 * const>(vector.data()) + bufferView.byteOffset, static_cast<i32>(bufferView.byteLength), &loadedWidth, &loadedHeight, &loadedChannels, 4);
            if (imageBytes == nullptr)
                Logger::error("Error loading texture: {}", stbi_failure_reason());
            m_textures.push_back(std::make_unique<Texture>(imageBytes, loadedWidth, loadedHeight, format, samplerInfo));
            return m_textures.back().get();
        };

        Texture *baseTexture = material.pbrData.baseColorTexture.has_value() ? resolveTexture(material.pbrData.baseColorTexture.value().textureIndex, vk::Format::eR8G8B8A8Srgb) : m_pureWhite1x1Texture;
        Texture *metallicRoughnessTexture = material.pbrData.metallicRoughnessTexture.has_value() ? resolveTexture(material.pbrData.metallicRoughnessTexture.value().textureIndex, vk::Format::eR8G8B8A8Unorm) : m_pureWhite1x1Texture;
        Texture *aoTexture = material.occlusionTexture.has_value() ? resolveTexture(material.occlusionTexture.value().textureIndex, vk::Format::eR8G8B8A8Unorm) : m_pureWhite1x1Texture;
        Texture *normalTexture = material.normalTexture.has_value() ? resolveTexture(material.normalTexture.value().textureIndex, vk::Format::eR8G8B8A8Unorm) : m_normal1x1Texture;
        m_materials.push_back(std::make_unique<Material>(baseTexture, metallicRoughnessTexture, aoTexture, normalTexture));
        materials.emplace_back(m_materials.back().get());
    }
    // meshes
    if (ctx->meshes.empty())
        Logger::warn("Loaded file contains no meshes");
    for (const auto & mesh : ctx->meshes) {
        m_meshes.push_back(loadMesh(ctx.get(), mesh));
        meshes.emplace_back(m_meshes.back().get());
    }

    // nodes
    if (ctx->scenes.size() > 1)
        Logger::warn("Loaded file contains more than one scene, only first one was loaded ({} total)", ctx->scenes.size());
    const fastgltf::Scene& scene = ctx->scenes.at(0);
    if (scene.nodeIndices.empty())
        Logger::warn("Loaded scene contained no nodes");

    ECS::Entity root;
    if (scene.nodeIndices.size() == 1) {
        root = -1;
    }
    else {
        root = ECS::createEntity();
        HierarchyComponent::addEmpty(root);
        ECS::addComponent<NamedComponent>(root, {path.string()});
        ECS::addComponent<Transform>(root, {});
    }
    std::stack<std::tuple<size_t, ECS::Entity>> nodesToVisit;
    for (size_t nodeID : scene.nodeIndices) {
        nodesToVisit.emplace(nodeID, root);
    }

    while (!nodesToVisit.empty()) {
        auto [nodeID, parent] = nodesToVisit.top();
        nodesToVisit.pop();
        const fastgltf::Node& node = ctx->nodes[nodeID];

        // create entity
        ECS::Entity entity = ECS::createEntity();
        if (root == -1) root = entity;

        // add model if it exists
        if (node.meshIndex.has_value()) {
            ECS::addComponent<Model3D>(entity, {meshes[node.meshIndex.value()], materials[ctx->meshes[node.meshIndex.value()].primitives[0].materialIndex.value()]});
        }

        Transform transform;
        // transform components are already specified
        if (std::holds_alternative<fastgltf::TRS>(node.transform)) {
            const auto& [translation, rotation, scale] = std::get<fastgltf::TRS>(node.transform);
            transform.position = glm::vec3(translation.x(), translation.y(), translation.z());
            transform.rotation = glm::quat(rotation.w(), rotation.x(), rotation.y(), rotation.z());
            transform.scale = glm::vec3(scale.x(), scale.y(), scale.z());
        }
        // transform components given as a matrix
        else {
            Logger::warn("Matrix transform specifiers are not supported yet");
        }

        ECS::addComponent<Transform>(entity, transform);
        ECS::addComponent<HierarchyComponent>(entity, {parent, {}});
        ECS::addComponent<NamedComponent>(entity, {std::string(node.name)});
        if (parent != -1) {
            ECS::getComponent<HierarchyComponent>(parent).children.push_back(entity);
        }
        for (size_t child : node.children) {
            nodesToVisit.emplace(child, entity);
        }
    }

    Transform::updateTransform(root);

    auto endTime = std::chrono::high_resolution_clock::now();

    Logger::info("Loaded '{}' in {} ms [read = {} ms]", path.string(), std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime).count(), std::chrono::duration_cast<std::chrono::milliseconds>(loadTime-startTime).count());

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
    auto skybox = std::make_unique<Skybox>(data, width, height);
    for (u32 i = 0; i < 6; ++i) {
        stbi_image_free(data[i]);
    }
    return std::move(skybox);
}

Mesh<> * AssetManager::getUnitCube() const {
    return m_unitCubeMesh;
}

std::unique_ptr<Mesh<>> AssetManager::loadMesh(const fastgltf::Asset& ctx, const fastgltf::Mesh &mesh) {
    if (mesh.primitives.size() > 1) {
        Logger::warn(std::format("Mesh loader does not currently support more than one 1 primitive per mesh, only the first was loaded ({} total)", mesh.primitives.size()));
    }
    const fastgltf::Primitive& primitive = mesh.primitives.at(0);

    const auto getAccessor = [&](const char* name) -> const fastgltf::Accessor& {
        auto* iterator = primitive.findAttribute(name);
        if (iterator == primitive.attributes.cend()) Logger::error("{} does not contain {} attribute", mesh.name, name);
        return ctx.accessors[iterator->accessorIndex];
    };

    // load vertex info
    const fastgltf::Accessor& positionAccessor = getAccessor("POSITION");
    const fastgltf::Accessor& uvAccessor = getAccessor("TEXCOORD_0");
    const fastgltf::Accessor& normalAccessor = getAccessor("NORMAL");
    const fastgltf::Accessor& tangentAccessor = getAccessor("TANGENT");
    std::vector<Vertex> vertices;
    vertices.reserve(positionAccessor.count);
    fastgltf::iterateAccessor<glm::vec3>(ctx, positionAccessor, [&](const glm::vec3& position) {
        vertices.emplace_back(position, glm::vec2(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
    });
    fastgltf::iterateAccessorWithIndex<glm::vec2>(ctx, uvAccessor, [&](const glm::vec2& uv, const size_t index) {
        vertices.at(index).uv = uv;
    });
    fastgltf::iterateAccessorWithIndex<glm::vec3>(ctx, normalAccessor, [&](const glm::vec3& normal, const size_t index) {
        vertices.at(index).normal = normal;
    });
    fastgltf::iterateAccessorWithIndex<glm::vec4>(ctx, tangentAccessor, [&](const glm::vec4& tangent, const size_t index) {
        vertices.at(index).tangent = glm::vec3(tangent);
    });

    // load indexes
    const fastgltf::Accessor& indexAccessor = ctx.accessors[primitive.indicesAccessor.value()];
    std::vector<u32> indexes;
    indexes.reserve(indexAccessor.count);
    fastgltf::iterateAccessor<u32>(ctx, indexAccessor, [&](const u32 index) {
        indexes.push_back(index);
    });

    return std::make_unique<Mesh<>>(vertices, indexes);
}

void AssetManager::loadImage(std::string path) {
    i32 width, height, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) throw std::runtime_error(std::format("Unable to load image at {} ({})", path, stbi_failure_reason()));
    m_textures.emplace_back(std::make_unique<Texture>(pixels, static_cast<u32>(width), static_cast<u32>(height)));
    stbi_image_free(pixels);
}