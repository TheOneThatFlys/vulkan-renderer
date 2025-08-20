#include "AssetManager.h"

#include <filesystem>
#include <bitset>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include "VulkanEngine.h"

Mesh::Mesh(VulkanEngine* engine, const tinygltf::Model& ctx, const tinygltf::Primitive& primitive) {
    m_engine = engine;
    // load indexes
    const tinygltf::Accessor& indexAccessor = ctx.accessors[primitive.indices];
    const tinygltf::BufferView& indexBufferView = ctx.bufferViews[indexAccessor.bufferView];
    const tinygltf::Buffer& indexBuffer = ctx.buffers[indexBufferView.buffer];

    m_indexCount = static_cast<u32>(indexAccessor.count);
    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        m_indexType = vk::IndexType::eUint32;
        std::vector<u32> indexes;
        size_t indexStart = indexAccessor.byteOffset + indexBufferView.byteOffset;
        indexes.reserve(indexAccessor.count);
        for (u32 i = 0; i < indexAccessor.count; ++i) {
            const auto index = *reinterpret_cast<const u32*>(&indexBuffer.data[indexStart + i * 4]);
            indexes.emplace_back(index);
        }
        createIndexBuffer(indexes);
    }
    else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        m_indexType = vk::IndexType::eUint16;
        std::vector<u16> indexes;
        size_t indexStart = indexAccessor.byteOffset + indexBufferView.byteOffset;
        indexes.reserve(indexAccessor.count);
        for (u32 i = 0; i < indexAccessor.count; ++i) {
            const auto index = *reinterpret_cast<const u16*>(&indexBuffer.data[indexStart + i * 2]);
            indexes.emplace_back(index);
        }
        createIndexBuffer(indexes);
    }
    else {
        Logger::error(std::format("Unknown index type: {}", indexAccessor.componentType));
    }

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

    createVertexBuffer(vertices);
}

Mesh::Mesh(VulkanEngine* engine, const std::vector<Vertex>& vertices, const std::vector<u32> &indexes) {
    m_engine = engine;

    createVertexBuffer(vertices);
    createIndexBuffer(indexes);

    m_indexCount = static_cast<u32>(indexes.size());
}

void Mesh::draw(const vk::raii::CommandBuffer& commandBuffer) const {
    commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(m_indexBuffer, 0, m_indexType);
    commandBuffer.drawIndexed(m_indexCount, 1, 0, 0, 0);
}

void Mesh::createVertexBuffer(const std::vector<Vertex>& vertices) {
    const u64 bufferSize = sizeof(Vertex) * vertices.size();

    // Logger::info(std::to_string(bufferSize));
    auto [stagingBuffer, stagingBufferMemory] = m_engine->createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
    );

    void* data = stagingBufferMemory.mapMemory(0, bufferSize, {});
    memcpy(data, vertices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();


    std::tie(m_vertexBuffer, m_vertexBufferMemory) = m_engine->createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    m_engine->copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
}

template<typename T>
void Mesh::createIndexBuffer(const std::vector<T>& indexes) {
    const u64 bufferSize = sizeof(T) * indexes.size();

    auto [stagingBuffer, stagingBufferMemory] = m_engine->createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
    );

    void* data = stagingBufferMemory.mapMemory(0, bufferSize, {});
    memcpy(data, indexes.data(), bufferSize);
    stagingBufferMemory.unmapMemory();

    std::tie(m_indexBuffer, m_indexBufferMemory) = m_engine->createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    m_engine->copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
}

void Mesh::validateAccessor(const tinygltf::Accessor &accessor, u32 componentType, u32 type) {
    if (accessor.componentType != componentType || accessor.type != type)
        Logger::error(std::format("Unexpected accessor format ({}, {}), expected ({}, {})", accessor.componentType, accessor.type, componentType, type));
}

Texture::Texture(const VulkanEngine *engine, const unsigned char *pixels, u32 width, u32 height, vk::Format format) {
    u32 bitdepth;
    switch (format) {
        case vk::Format::eR8G8B8Unorm:
            bitdepth = 3;
        default:
            bitdepth = 4;
    }

    vk::DeviceSize imageSize = width * height * bitdepth;

    auto [stagingBuffer, stagingBufferMemory] = engine->createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    std::tie(m_image, m_imageMemory) = engine->createImage(
        width, height,
        format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    // transition image layout for optimal copying
    engine->transitionImageLayout(m_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    // copy staging buffer data to image
    engine->copyBufferToImage(stagingBuffer, m_image, width, height);
    // transition to format optimal for shader reads
    engine->transitionImageLayout(m_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::ImageViewCreateInfo viewInfo = {
        .image = m_image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    m_imageView = vk::raii::ImageView(engine->getDevice(), viewInfo);

    vk::SamplerCreateInfo samplerInfo = {
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = engine->getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .unnormalizedCoordinates = vk::False
    };

    m_sampler = vk::raii::Sampler(engine->getDevice(), samplerInfo);
}

const vk::raii::Image & Texture::getImage() const {
    return m_image;
}

const vk::raii::DeviceMemory & Texture::getImageMemory() const {
    return m_imageMemory;
}

const vk::raii::ImageView & Texture::getImageView() const {
    return m_imageView;
}

const vk::raii::Sampler & Texture::getSampler() const {
    return m_sampler;
}

AssetManager::AssetManager(VulkanEngine* engine) {
    m_engine = engine;
}

void AssetManager::load(const char* root) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const std::filesystem::path& filepath = entry.path();

        std::string path = filepath.generic_string();
        std::string extension = filepath.extension().generic_string();

        // model
        if (extension == ".glb") {
            Logger::info(std::format("Loading model at {}", path));
            loadGLB(path);
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

const std::vector<std::unique_ptr<Model>>& AssetManager::getModels() const {
    return m_models;
}

const Texture * AssetManager::getTexture(const std::string& key) const {
    try {
        return m_textures.at(key).get();
    }
    catch (std::out_of_range e) {
        Logger::error(std::format("Could not find texture with key '{}'", key));
        return nullptr;
    }
}

const Material * AssetManager::getMaterial(const std::string &key) const {
    try {
        return m_materials.at(key).get();
    }
    catch (std::out_of_range e) {
        Logger::error(std::format("Could not find material with key '{}'", key));
        return nullptr;
    }
}

const Mesh * AssetManager::getMesh(const std::string &key) const {
    try {
        return m_meshes.at(key).get();
    }
    catch (std::out_of_range e) {
        Logger::error(std::format("Could not find mesh with key '{}'", key));
        return nullptr;
    }
}

void AssetManager::loadGLB(const std::string& path) {
    tinygltf::Model ctx;
    std::string error, warning;
    m_loader.LoadBinaryFromFile(&ctx, &error, &warning, path);
    if (!error.empty())
        Logger::error(error);
    if (!warning.empty())
        Logger::warn(warning);

    auto key = [&](u32 n) -> std::string {
        return path + std::to_string(n);
    };

    // textures
    for (u32 i = 0; i < ctx.textures.size(); ++i) {
        const tinygltf::Texture& texture = ctx.textures[i];
        const tinygltf::Sampler& sampler = ctx.samplers[texture.sampler];
        const tinygltf::Image& source = ctx.images[texture.source];
        m_textures[key(i)] = std::make_unique<Texture>(m_engine, source.image.data(), source.width, source.height);
    }
    // materials
    for (u32 i = 0; i < ctx.materials.size(); ++i) {
        const tinygltf::Material& material = ctx.materials[i];
        m_materials[key(i)] = std::make_unique<Material>(
            getTexture(key(material.pbrMetallicRoughness.baseColorTexture.index))
        );
    }
    // meshes
    for (u32 i = 0; i < ctx.meshes.size(); ++i) {
        const tinygltf::Mesh& mesh = ctx.meshes[i];
        if (mesh.primitives.size() > 1) {
            Logger::warn(std::format("Mesh loader does not currently support more than one 1 primitive per mesh, only the first was loaded ({} total)", mesh.primitives.size()));
        }
        const tinygltf::Primitive& primitive = mesh.primitives.at(0);
        m_meshes[key(i)] = std::make_unique<Mesh>(m_engine, ctx, primitive);
        m_models.push_back(std::make_unique<Model>(getMesh(key(i)), getMaterial(key(primitive.material))));
    }

}

void AssetManager::loadImage(std::string path) {
    i32 width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) throw std::runtime_error(std::format("Unable to load image at {} ({})", path, stbi_failure_reason()));
    m_textures[path] = std::make_unique<Texture>(m_engine, pixels, static_cast<u32>(width), static_cast<u32>(height));
    stbi_image_free(pixels);
}

std::string AssetManager::generateLocalKey(std::string path, u32 id) {
    return path + std::to_string(id);
}
