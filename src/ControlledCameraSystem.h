#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Components.h"
#include "ECS.h"

class ControlledCameraSystem : public ECS::System, public IUpdatable {
public:
    explicit ControlledCameraSystem(GLFWwindow* window);
    void update(float deltaTime) override;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

private:
    ControlledCamera& getCamera() const;
    GLFWwindow* m_window;
    float m_aspect = 1.0f;
};
