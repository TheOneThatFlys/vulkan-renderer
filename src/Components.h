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
    u32 level;
};
