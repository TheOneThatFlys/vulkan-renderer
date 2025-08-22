#include "Mesh.h"

#include "VulkanEngine.h"

Mesh::Mesh(VulkanEngine* engine, const std::vector<Vertex>& vertices, const std::vector<u32> &indexes): m_engine(engine), m_indexCount(static_cast<u32>(indexes.size())), m_indexType(vk::IndexType::eUint32) {
    createVertexBuffer(vertices);
    createIndexBuffer(indexes);
}

Mesh::Mesh(VulkanEngine* engine, const std::vector<Vertex>& vertices, const std::vector<u16> &indexes) : m_engine(engine), m_indexCount(static_cast<u32>(indexes.size())), m_indexType(vk::IndexType::eUint16) {
    createVertexBuffer(vertices);
    createIndexBuffer(indexes);
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
    std::memcpy(data, indexes.data(), bufferSize);
    stagingBufferMemory.unmapMemory();

    std::tie(m_indexBuffer, m_indexBufferMemory) = m_engine->createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    m_engine->copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
}
