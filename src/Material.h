#pragma once

#include <glm/glm.hpp>

#include "Texture.h"

class Material {
public:
    Material(const Texture *base, const Texture *metallicRoughness, const Texture *ao, const Texture *normal);
    void use(const vk::raii::CommandBuffer& commandBuffer, const vk::raii::PipelineLayout& layout) const;

private:
    const Texture *m_base, *m_metallicRoughness, *m_ao, *m_normal;
    glm::vec3 m_baseMulti, m_metallicRoughnessMulti, m_aoMulti;
    vk::raii::DescriptorSet m_descriptorSet = nullptr;
};