#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ECS.h"
#include "Material.h"
#include "Mesh.h"

struct Transform {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    glm::mat4 transform;

    ECS::Entity parent;
};

struct Model3D {
    Mesh* mesh;
    Material* material;
};
