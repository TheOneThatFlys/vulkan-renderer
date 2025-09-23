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
    static void addChild(ECS::Entity parent, ECS::Entity child);

    // generate a hierarchy component with no parent and add to child
    static void addEmpty(ECS::Entity child);

    // move a child component to a new parent
    static void move(ECS::Entity newParent, ECS::Entity child);
};

struct BoundingVolume {
    OBB obb;

    static BoundingVolume from(ECS::Entity entity);
};

struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::mat4 transform = glm::mat4(1.0f);

    static glm::vec3 getTransform(const glm::mat4& matrix);
    static glm::vec3 getScale(const glm::mat4& matrix);
    static glm::quat getRotation(const glm::mat4& matrix);
    static void updateTransform(ECS::Entity entity);
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

    float near = 0.01f;
    float far = 100.0f;
};

struct PointLight {
    glm::vec3 colour;
    float strength;
};