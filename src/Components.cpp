#include "Components.h"

void HierarchyComponent::addChild(const ECS::Entity parent, const ECS::Entity child) {
    assert(parent >= 0 && child >= 0);
    assert(!ECS::hasComponent<HierarchyComponent>(child));
    assert(ECS::hasComponent<HierarchyComponent>(parent));
    auto &parentHierarchy = ECS::getComponent<HierarchyComponent>(parent);
    parentHierarchy.children.push_back(child);
    ECS::addComponent<HierarchyComponent>(child, {parent, {}});
}

void HierarchyComponent::addEmpty(const ECS::Entity child) {
    assert(child >= 0);
    ECS::addComponent<HierarchyComponent>(child, {-1, {}});
}

void HierarchyComponent::move(const ECS::Entity newParent, const ECS::Entity child) {
    assert(ECS::hasComponent<HierarchyComponent>(child));
    assert(ECS::hasComponent<HierarchyComponent>(newParent));

    auto &childHierarchy = ECS::getComponent<HierarchyComponent>(child);
    if (childHierarchy.parent != -1) {
        auto& parentsChildren = ECS::getComponent<HierarchyComponent>(childHierarchy.parent).children;
        std::erase(parentsChildren, child);
    }

    auto &parentHierarchy = ECS::getComponent<HierarchyComponent>(newParent);
    parentHierarchy.children.push_back(child);
    childHierarchy.parent = newParent;
}


BoundingVolume BoundingVolume::from(const ECS::Entity entity) {
    assert(ECS::hasComponent<Transform>(entity));
    assert(ECS::hasComponent<Model3D>(entity));

    const auto& transform = ECS::getComponent<Transform>(entity);
    const auto& model = ECS::getComponent<Model3D>(entity);

    const glm::vec3 globalScale = Transform::getScale(transform.transform);
    const auto localOBB = model.mesh->getLocalOBB();
    const auto scaledCenter = glm::vec3(
        glm::dot(glm::vec3(transform.transform[0][0], transform.transform[1][0], transform.transform[2][0]), localOBB.center),
        glm::dot(glm::vec3(transform.transform[0][1], transform.transform[1][1], transform.transform[2][1]), localOBB.center),
        glm::dot(glm::vec3(transform.transform[0][2], transform.transform[1][2], transform.transform[2][2]), localOBB.center)
    );

    return BoundingVolume{{
        .center = Transform::getTransform(transform.transform) + scaledCenter,
        .extent = globalScale * localOBB.extent,
        .rotation = Transform::getRotation(transform.transform)
    }};
}


glm::vec3 Transform::getTransform(const glm::mat4& matrix) {
    return glm::vec3(matrix[3]);
}

glm::vec3 Transform::getScale(const glm::mat4& matrix) {
    return {
        glm::length(matrix[0]),
        glm::length(matrix[1]),
        glm::length(matrix[2])
    };
}

glm::quat Transform::getRotation(const glm::mat4& matrix) {
    glm::mat4 matCpy = matrix;
    const auto scale = getScale(matrix);
    matCpy[0] /= scale.x;
    matCpy[1] /= scale.y;
    matCpy[2] /= scale.z;
    return glm::quat_cast(matCpy);
}

void Transform::updateTransform(const ECS::Entity entity) {
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

    if (ECS::hasComponent<BoundingVolume>(entity)) {
        ECS::getComponent<BoundingVolume>(entity) = BoundingVolume::from(entity);
    }
    else if (ECS::hasComponent<Model3D>(entity)) {
        ECS::addComponent(entity, BoundingVolume::from(entity));
    }

    if (hierarchy) {
        for (const ECS::Entity child : hierarchy->children) {
            if (ECS::hasComponent<Transform>(child)) {
                updateTransform(child);
            }
        }
    }
}