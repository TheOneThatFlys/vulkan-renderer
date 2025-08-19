#include "AssetManager.h"

#include <filesystem>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include "VulkanEngine.h"

const std::vector<Vertex> g_vertices = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 1.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}}
};
const std::vector<u32> g_indexes = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

Mesh::Mesh(VulkanEngine* engine, const tinygltf::Model& ctx, const tinygltf::Primitive& primitive) {
    m_engine = engine;
    // load indexes
    const tinygltf::Accessor& indexAccessor = ctx.accessors[primitive.indices];
    const tinygltf::BufferView& indexBufferView = ctx.bufferViews[indexAccessor.bufferView];
    const tinygltf::Buffer& indexBuffer = ctx.buffers[indexBufferView.buffer];

    validateAccessor(indexAccessor, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, TINYGLTF_TYPE_SCALAR);

    size_t indexStart = indexBuffer.data.front() + indexAccessor.byteOffset + indexBufferView.byteOffset;
    std::vector<u32> indexes;
    indexes.reserve(indexAccessor.count);
    for (u32 i = 0; i < indexAccessor.count; ++i) {
        const auto index = *reinterpret_cast<const u32*>(&indexBuffer.data[indexStart + i * 4]);
        indexes.emplace_back(index);
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
    size_t positionStart = positionBuffer.data.front() + positionAccessor.byteOffset + positionBufferView.byteOffset;
    size_t uvStart = uvBuffer.data.front() + uvAccessor.byteOffset + uvBufferView.byteOffset;
    for (u32 i = 0; i < positionAccessor.count; ++i) {
        const auto pos = reinterpret_cast<const float*>(&positionBuffer.data[positionStart + i * 12]);
        const auto uv = reinterpret_cast<const float*>(&uvBuffer.data[uvStart + i * 8]);
        vertices.emplace_back(glm::vec3(pos[0], pos[1], pos[2]), glm::vec2(uv[0], uv[1]));
    }

    // create buffers
    createVertexBuffer(vertices);
    createIndexBuffer(indexes);

    m_indexCount = static_cast<u32>(indexAccessor.count);
}

Mesh::Mesh(VulkanEngine* engine, const std::vector<Vertex>& vertices, const std::vector<u32> &indexes) {
    m_engine = engine;

    createVertexBuffer(vertices);
    createIndexBuffer(indexes);

    m_indexCount = static_cast<u32>(indexes.size());
}

void Mesh::draw(const vk::raii::CommandBuffer& commandBuffer) const {
    commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(m_indexBuffer, 0, vk::IndexType::eUint32);
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

void Mesh::createIndexBuffer(const std::vector<u32>& indexes) {
    const u64 bufferSize = sizeof(u32) * indexes.size();

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



AssetManager::AssetManager(VulkanEngine* engine) {
    m_engine = engine;
}

void AssetManager::load(const char* root) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        std::filesystem::path filepath = entry.path();

        // Skip non .glb files
        if (filepath.extension() != ".glb") continue;
        // Convert filepath to string
        std::string path = filepath.generic_string();
        Logger::info(std::format("Loading model at {}", path));
        loadGLB(path);
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    Logger::info(std::format("Loaded assets in {} ms", duration.count()));
}

std::vector<std::unique_ptr<Mesh>>& AssetManager::getMeshes() {
    return m_meshes;
}

void AssetManager::loadGLB(std::string path) {
    tinygltf::Model ctx;
    std::string error, warning;
    m_loader.LoadBinaryFromFile(&ctx, &error, &warning, path);
    if (!error.empty())
        Logger::error(error);
    if (!warning.empty())
        Logger::warn(warning);

    for (const auto& mesh : ctx.meshes) {
        if (mesh.primitives.size() > 1) {
            Logger::warn(std::format("Mesh loader does not currently support more than one 1 primitive per mesh, only the first was loaded ({} total)", mesh.primitives.size()));
        }
        const tinygltf::Primitive& primitive = mesh.primitives.at(0);
        m_meshes.push_back(std::make_unique<Mesh>(m_engine, ctx, primitive));
    }
}
