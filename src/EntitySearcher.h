#pragma once

#include <optional>

#include "ECS.h"

class EntitySearcher final : public ECS::System {
public:
    std::optional<ECS::Entity> find(const std::string & name) const;
    static std::optional<ECS::Entity> findChild(const std::string & name, ECS::Entity initialNode);
};

class AllEntities final : public ECS::System {
public:
    const std::unordered_set<ECS::Entity>& get();
};
