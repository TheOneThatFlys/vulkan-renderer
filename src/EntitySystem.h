#pragma once

#include "ECS.h"

class EntitySystem final : public ECS::System {
public:
    const std::unordered_set<ECS::Entity>& get();
};
