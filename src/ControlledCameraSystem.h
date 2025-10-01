#pragma once

#include <glm/glm.hpp>

#include "Components.h"
#include "ECS.h"
#include "Volumes.h"

struct CameraVectors {
    glm::vec3 front, right, up;
};

class ControlledCameraSystem : public ECS::System, public IUpdatable {
public:
    explicit ControlledCameraSystem();
    void update(float deltaTime) override;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    Frustum getFrustum() const;
    // get normalized vector
    glm::vec3 getFrontVector() const;
    CameraVectors getVectors() const;
    Ray normalisedScreenToRay(glm::vec2 normalisedScreenCoordinates) const;

private:
    ControlledCamera& getCamera() const;
};
