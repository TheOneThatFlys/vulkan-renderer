#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> descriptions;
        // position
        descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sfloat,
            .offset = offsetof(Vertex, pos)
        };
        // color
        descriptions[1] = {
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = offsetof(Vertex, uv),
        };
        return descriptions;
    }
};
