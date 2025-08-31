#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ECS.h"
#include "Material.h"
#include "Mesh.h"

struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::mat4 transform = glm::mat4(1.0f);
};

struct HierarchyComponent {
    ECS::Entity parent;
    std::vector<ECS::Entity> children;
    u32 level;

    // generate a hierarchy component from a parent and add it onto the child
    static void addChild(const ECS::Entity parent, const ECS::Entity child) {
        assert(parent >= 0 && child >= 0);
        assert(!ECS::hasComponent<HierarchyComponent>(child));
        assert(ECS::hasComponent<HierarchyComponent>(parent));
        auto &parentHierarchy = ECS::getComponent<HierarchyComponent>(parent);
        parentHierarchy.children.push_back(child);
        ECS::addComponent<HierarchyComponent>(child, {parent, {}, parentHierarchy.level + 1});
    }

    // generate a hierarchy component with no parent and add to child
    static void addEmpty(const ECS::Entity child) {
        assert(child >= 0);
        ECS::addComponent<HierarchyComponent>(child, {-1, {}, 0});
    }

    // // move a child component to a new parent
    // static void move(const ECS::Entity newParent, const ECS::Entity child) {
    //     assert(ECS::hasComponent<HierarchyComponent>(child));
    //     assert(ECS::hasComponent<HierarchyComponent>(newParent));
    //
    //     auto &childHierarchy = ECS::getComponent<HierarchyComponent>(child);
    //     if (childHierarchy.parent != -1) {
    //         auto& parentsChildren = ECS::getComponent<HierarchyComponent>(childHierarchy.parent).children;
    //         std::erase(parentsChildren, child);
    //     }
    //
    //     auto &parentHierarchy = ECS::getComponent<HierarchyComponent>(newParent);
    //     parentHierarchy.children.push_back(child);
    //     childHierarchy.parent = newParent;
    //
    //     // we need to resolve levels recursively
    //     childHierarchy.level = parentHierarchy.level + 1;
    //
    // }
};

struct Model3D {
    Mesh* mesh = nullptr;
    Material* material = nullptr;
};

struct NamedComponent {
    std::string name;
};

struct ControlledCamera {
    glm::vec3 position = glm::vec3(0.0f);
    float speed = 2.0f;
    float fov = glm::radians(45.0f);
    float yaw = glm::radians(-90.0f);
    float pitch = glm::radians(0.0f);

    float sensitivity = 0.001f;
    float aspect = 1.0f;

    bool capturingMouse = true;
};

struct PointLight {
    glm::vec3 colour;
    float strength;
};