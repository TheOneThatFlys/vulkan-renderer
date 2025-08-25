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

struct Model3D {
    Mesh* mesh;
    Material* material;
};

struct HierarchyComponent {
    ECS::Entity parent;
    std::vector<ECS::Entity> children;
    u32 level;
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