#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ECS.h"
#include "Material.h"
#include "Mesh.h"

// these macros are defined in windows.h
// hopefully undef-ing them doesn't break anything...
#undef near
#undef far

struct HierarchyComponent {
    ECS::Entity parent;
    std::vector<ECS::Entity> children;

    // generate a hierarchy component from a parent and add it onto the child
    static void addChild(const ECS::Entity parent, const ECS::Entity child) {
        assert(parent >= 0 && child >= 0);
        assert(!ECS::hasComponent<HierarchyComponent>(child));
        assert(ECS::hasComponent<HierarchyComponent>(parent));
        auto &parentHierarchy = ECS::getComponent<HierarchyComponent>(parent);
        parentHierarchy.children.push_back(child);
        ECS::addComponent<HierarchyComponent>(child, {parent, {}});
    }

    // generate a hierarchy component with no parent and add to child
    static void addEmpty(const ECS::Entity child) {
        assert(child >= 0);
        ECS::addComponent<HierarchyComponent>(child, {-1, {}});
    }

    // move a child component to a new parent
    static void move(const ECS::Entity newParent, const ECS::Entity child) {
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
};

struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::mat4 transform = glm::mat4(1.0f);

    static glm::vec3 getTransform(const glm::mat4& matrix) {
        return glm::vec3(matrix[3]);
    }

    static glm::vec3 getScale(const glm::mat4& matrix) {
        return {
            glm::length(matrix[0]),
            glm::length(matrix[1]),
            glm::length(matrix[2])
        };
    }

    static glm::quat getRotation(const glm::mat4& matrix) {
        glm::mat4 matCpy = matrix;
        const auto scale = getScale(matrix);
        matCpy[0] /= scale.x;
        matCpy[1] /= scale.y;
        matCpy[2] /= scale.z;
        return glm::quat_cast(matCpy);
    }

    static void updateTransform(const ECS::Entity entity) {
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
                    updateTransform(child);
                }
            }
        }
    }
};

struct Model3D {
    Mesh<>* mesh = nullptr;
    Material* material = nullptr;
};

struct NamedComponent {
    std::string name;
};

struct ControlledCamera {
    glm::vec3 position = glm::vec3(0.0f);
    float speed = 2.0f;
    float fov = glm::radians(70.0f);
    float yaw = glm::radians(-90.0f);
    float pitch = glm::radians(0.0f);

    float sensitivity = 0.001f;
    float aspect = 1.0f;

    bool capturingMouse = true;

    float near = 0.1f;
    float far = 100.0f;
};

struct PointLight {
    glm::vec3 colour;
    float strength;
};