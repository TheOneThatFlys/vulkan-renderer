#include "TransformSystem.h"

#include "Components.h"

void TransformSystem::update(float) {

    // first loop through entities and sort by layer
    std::vector<std::vector<ECS::Entity>> layers;
    for (const ECS::Entity entity : m_entities) {
        u32 layer = 0;
        if (ECS::hasComponent<HierarchyComponent>(entity)) {
            layer = ECS::getComponent<HierarchyComponent>(entity).level;
        }

        if (layer >= layers.size()) {
            layers.emplace_back();
        }
        layers.at(layer).push_back(entity);
    }
    // loop through layers in order and resolve - this ensures transformations are resolved properly
    for (u32 layerNumber = 0; layerNumber < layers.size(); ++layerNumber) {
        for (const ECS::Entity entity : layers.at(layerNumber)) {
            auto& [position, rotation, scale, transform] = ECS::getComponent<Transform>(entity);
            transform = glm::translate(glm::mat4(1.0f), position);
            transform = glm::mat4_cast(rotation) * transform;
            transform = glm::scale(transform, scale);

            if (layerNumber > 0 && ECS::hasComponent<HierarchyComponent>(entity)) {
                const ECS::Entity parent = ECS::getComponent<HierarchyComponent>(entity).parent;
                transform = ECS::getComponent<Transform>(parent).transform * transform;
            }
        }
    }

}
