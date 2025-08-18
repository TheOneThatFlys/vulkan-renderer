#include "AssetManager.h"

#include <filesystem>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include "VulkanEngine.h"

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 1.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}}
};
const std::vector<u16> indexes = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

Mesh::Mesh(VulkanEngine* engine, const std::vector<glm::vec3> &positions, const std::vector<glm::vec2> &uvs, const std::vector<u16> &indexes) {

}

Mesh::Mesh(VulkanEngine* engine, const std::vector<Vertex>& vertices, const std::vector<u16> &indexes) {
    m_engine = engine;

    createVertexBuffer(vertices);
    createIndexBuffer(indexes);

    m_indexCount = static_cast<u32>(indexes.size());
}

void Mesh::draw(const vk::raii::CommandBuffer& commandBuffer) const {
    commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(m_indexBuffer, 0, vk::IndexType::eUint16);
    commandBuffer.drawIndexed(m_indexCount, 1, 0, 0, 0);
}

void Mesh::createVertexBuffer(const std::vector<Vertex>& vertices) {
    const u64 bufferSize = sizeof(Vertex) * vertices.size();

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

void Mesh::createIndexBuffer(const std::vector<u16>& indexes) {
    const u64 bufferSize = sizeof(u16) * indexes.size();

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

    m_meshes.emplace_back(m_engine, vertices, indexes);

    const auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    Logger::info(std::format("Loaded assets in {} ms", duration.count()));
}

std::vector<Mesh>& AssetManager::getMeshes() {
    return m_meshes;
}

void AssetManager::loadGLB(std::string path) {
    return;

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

        const tinygltf::Accessor& indexAccessor = ctx.accessors[primitive.indices];
        const tinygltf::BufferView& indexBufferView = ctx.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = ctx.buffers[indexBufferView.buffer];

        size_t start = indexAccessor.byteOffset + indexBufferView.byteOffset;
        size_t length = indexBufferView.byteLength;
        auto indexes = std::vector<float>();
        auto indexData = indexBuffer.data;
    }
}

tinygltf::Buffer& AssetManager::getDataFromAccessor(tinygltf::Model ctx, u32 index) {
    const tinygltf::Accessor& accessor = ctx.accessors[index];
    const tinygltf::BufferView& bufferView = ctx.bufferViews[accessor.bufferView];
    tinygltf::Buffer& buffer = ctx.buffers[bufferView.buffer];

    Logger::info(std::to_string(accessor.count));
    return buffer;
}
