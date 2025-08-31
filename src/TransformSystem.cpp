#include "TransformSystem.h"

#include <stack>

#include "Components.h"

void TransformSystem::update(float) {
    std::stack<ECS::Entity> toVisit;
    for (const ECS::Entity entity : m_entities) {
        const auto hierarchy = ECS::getComponentOptional<HierarchyComponent>(entity);
        if (!hierarchy || hierarchy->parent == -1) {
            toVisit.emplace(entity);
        }
    }

    while (!toVisit.empty()) {
        // we are not guaranteed that this entity will have a hierarchy component, but it will always have a transform component
        ECS::Entity entity = toVisit.top();
        toVisit.pop();

        const auto hierarchy = ECS::getComponentOptional<HierarchyComponent>(entity);
        auto& [position, rotation, scale, transform] = ECS::getComponent<Transform>(entity);

        transform = glm::translate(glm::mat4(1.0f), position);
        transform = transform * glm::mat4_cast(rotation);
        transform = glm::scale(transform, scale);

        if (hierarchy && hierarchy->parent != -1) {
            const auto parentTransform = ECS::getComponentOptional<Transform>(hierarchy->parent);
            if (parentTransform) {
                transform = parentTransform->transform * transform;
            }
        }

        if (hierarchy) {
            for (const ECS::Entity child : hierarchy->children) {
                if (ECS::hasComponent<Transform>(child)) {
                    toVisit.emplace(child);
                }
            }
        }
    }

}
