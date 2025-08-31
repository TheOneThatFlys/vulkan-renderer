#include "EntitySearcher.h"

#include <stack>

#include "Components.h"

// search in global space for a component
std::optional<ECS::Entity> EntitySearcher::find(const std::string & name) const {
    for (ECS::Entity entity : m_entities) {
        if (ECS::getComponent<NamedComponent>(entity).name == name) {
            return std::optional(entity);
        }
    }
    return std::optional<ECS::Entity>();
}

std::optional<ECS::Entity> EntitySearcher::findChild(const std::string & name, const ECS::Entity initialNode) {
    assert(ECS::hasComponent<NamedComponent>(initialNode) && ECS::hasComponent<HierarchyComponent>(initialNode));

    std::stack<ECS::Entity> visitStack;
    visitStack.push(initialNode);

    while (!visitStack.empty()) {
        ECS::Entity entity = visitStack.top();
        visitStack.pop();

        if (ECS::hasComponent<NamedComponent>(entity) && ECS::getComponent<NamedComponent>(entity).name == name) {
            return entity;
        }

        if (ECS::hasComponent<HierarchyComponent>(entity)) {
            for (const ECS::Entity child : ECS::getComponent<HierarchyComponent>(entity).children) {
                visitStack.push(child);
            }
        }
    }

    return std::optional<ECS::Entity>();

}
