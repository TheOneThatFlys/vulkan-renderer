#include "EntitySystem.h"

const std::unordered_set<ECS::Entity>& EntitySystem::get() {
    return m_entities;
}
