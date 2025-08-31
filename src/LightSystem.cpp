#include "LightSystem.h"

std::array<LightSystem::PointLightFragData, LightSystem::MAX_LIGHTS> LightSystem::getLights(u32 &number) const {
    std::array<PointLightFragData, MAX_LIGHTS> ret{};
    u32 i = 0;
    for (const ECS::Entity entity : m_entities) {
        auto& element = ret.at(i++);
        auto& lightData = ECS::getComponent<PointLight>(entity);
        auto& positionData = ECS::getComponent<Transform>(entity);
        element.colour = lightData.colour;
        element.strength = lightData.strength;
        // get the full transformed (parented) translation data from the matrix
        element.position = glm::vec3(positionData.transform[3]);
        if (i == MAX_LIGHTS) break;
    }
    number = i;
    return ret;
}
