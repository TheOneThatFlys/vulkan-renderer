#include "Mesh.h"

#include "VulkanEngine.h"

template<ValidVertex V>
Mesh<V>::Mesh(VulkanEngine* engine, const std::vector<V>& vertices, const std::vector<u32> &indexes)
    : m_engine(engine)
    , m_indexCount(static_cast<u32>(indexes.size()))
    , m_indexType(vk::IndexType::eUint32)
    , m_localOBB(resolveOBB(vertices))
{
    createVertexBuffer(vertices);
    createIndexBuffer(indexes);
}

template<ValidVertex V>
void Mesh<V>::draw(const vk::raii::CommandBuffer& commandBuffer) const {
    commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(m_indexBuffer, 0, m_indexType);
    commandBuffer.drawIndexed(m_indexCount, 1, 0, 0, 0);
}

template<ValidVertex V>
OBB Mesh<V>::getLocalOBB() const {
    return m_localOBB;
}

template<ValidVertex V>
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

template<ValidVertex V>
OBB Mesh<V>::resolveOBB(const std::vector<V>& vertices) {
    auto max = glm::vec3(-std::numeric_limits<float>::max());
    auto min = glm::vec3(std::numeric_limits<float>::max());
    for (const auto& vertex : vertices) {
        if (vertex.pos.x > max.x) {
            max.x = vertex.pos.x;
        }
        if (vertex.pos.x < min.x) {
            min.x = vertex.pos.x;
        }

        if (vertex.pos.y > max.y) {
            max.y = vertex.pos.y;
        }
        if (vertex.pos.y < min.y) {
            min.y = vertex.pos.y;
        }

        if (vertex.pos.z > max.z) {
            max.z = vertex.pos.z;
        }
        if (vertex.pos.z < min.z) {
            min.z = vertex.pos.z;
        }
    }
    assert(max.x >= min.x && max.y >= min.y && max.z >= min.z);
    return {
        .center = (max + min) / 2.0f,
        .extent = (max - min) / 2.0f,
        .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
    };
}

template<ValidVertex V>
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