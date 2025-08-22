#include "Scene.h"

#include <stack>

Scene::Scene() {
    type = eScene;
}

Scene::~Scene() {}

std::vector<const Instance*> Scene::getRenderable() const {
    std::vector<const Instance*> instances;
    std::stack<const Node*> nodesToVisit;
    nodesToVisit.push(this);
    while (!nodesToVisit.empty()) {
        const Node* node = nodesToVisit.top();
        nodesToVisit.pop();

        if (node->type == eInstance) {
            auto instance = reinterpret_cast<const Instance*>(node);
            instances.push_back(instance);
        }

        for (const auto& child : node->getChildren()) {
            nodesToVisit.emplace(child.get());
        }
    }
    return instances;
}
