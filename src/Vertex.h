#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>

struct BasicVertex {
    glm::vec3 pos;
    static vk::VertexInputBindingDescription getBindingDescription() {
        return {
            .binding = 0,
            .stride = sizeof(BasicVertex),
            .inputRate = vk::VertexInputRate::eVertex
        };
    }

    static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<vk::VertexInputAttributeDescription> descriptions;
        // position
        descriptions.push_back({
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(BasicVertex, pos)
        });
        return descriptions;
    }
};

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec3 tangent;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };
    }

    static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<vk::VertexInputAttributeDescription> descriptions;
        // position
        descriptions.push_back({
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, pos)
        });
        // color
        descriptions.push_back({
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = offsetof(Vertex, uv)
        });
        // normal
        descriptions.push_back({
            .location = 2,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, normal)
        });
        // tangent
        descriptions.push_back({
            .location = 3,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, tangent)
        });
        return descriptions;
    }
};
