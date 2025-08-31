#pragma once

#include <optional>

#include "ECS.h"

class EntitySearcher : public ECS::System {
public:
    std::optional<ECS::Entity> find(const std::string & name) const;
    static std::optional<ECS::Entity> findChild(const std::string & name, ECS::Entity initialNode);
};
