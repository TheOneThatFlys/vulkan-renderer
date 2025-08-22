#pragma once

#include "Node.h"
#include "Instance.h"

class Scene : public Node {
public:
    Scene();
    ~Scene();
    std::vector<const Instance*> getRenderable() const;
};
