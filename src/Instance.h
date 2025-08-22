#pragma once

#include "Mesh.h"
#include "Material.h"
#include "Node.h"

class Instance : public Node {
public:
    Instance(Mesh* mesh, Material* material, glm::vec3 position = glm::vec3(0.0f), glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3 scale = glm::vec3(1.0f));
    const Mesh* mesh;
    const Material* material;

};