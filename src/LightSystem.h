#pragma once

#include "ECS.h"
#include "Components.h"


class LightSystem : public ECS::System {
public:
    struct PointLightFragData {
        alignas (16) glm::vec3 position;
        alignas (16) glm::vec3 colour;
        alignas (4) float strength;
    };

    static constexpr u32 MAX_LIGHTS = 4;
    std::array<PointLightFragData, MAX_LIGHTS> getLights(u32 &number) const;
};
