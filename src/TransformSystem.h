#pragma once

#include "ECS.h"

class TransformSystem : public ECS::System, public IUpdatable {
    void update(float) override;
};
