#include "Instance.h"

Instance::Instance(Mesh* mesh, Material* material, glm::vec3 position, glm::quat rotation, glm::vec3 scale) : Node(position, rotation, scale), mesh(mesh), material(material) {
    type = eInstance;
}
