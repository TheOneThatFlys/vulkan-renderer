#include "Mesh.h"

#include "VulkanEngine.h"

template<typename V>
Mesh<V>::Mesh(VulkanEngine* engine, const std::vector<V>& vertices, const std::vector<u32> &indexes)
    : m_engine(engine)
    , m_indexCount(static_cast<u32>(indexes.size()))
    , m_indexType(vk::IndexType::eUint32)
    , m_maxDistance(resolveMaxDistance(vertices))
{
    createVertexBuffer(vertices);
    createIndexBuffer(indexes);
}

template<typename V>
void Mesh<V>::draw(const vk::raii::CommandBuffer& commandBuffer) const {
    commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(m_indexBuffer, 0, m_indexType);
    commandBuffer.drawIndexed(m_indexCount, 1, 0, 0, 0);
}

template<typename V>
float Mesh<V>::getMaxDistance() const {
    return m_maxDistance;
}

template<typename V>
void Mesh<V>::createVertexBuffer(const std::vector<V>& vertices) {
    const u64 bufferSize = sizeof(V) * vertices.size();

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

template<typename V>
float Mesh<V>::resolveMaxDistance(const std::vector<V>& vertices) {
    float maxDistanceSquared = -1.0f;
    for (const auto& vertex : vertices) {
        float distanceSquared = vertex.pos.x  * vertex.pos.x + vertex.pos.y * vertex.pos.y + vertex.pos.z * vertex.pos.z;
        if (distanceSquared > maxDistanceSquared) {
            maxDistanceSquared = distanceSquared;
        }
    }
    return std::sqrt(maxDistanceSquared);
}

template<typename V>
void Mesh<V>::createIndexBuffer(const std::vector<u32>& indexes) {
    const u64 bufferSize = sizeof(u32) * indexes.size();

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

template class Mesh<Vertex>;
template class Mesh<BasicVertex>;

