#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class CameraController {
public:
    explicit CameraController(GLFWwindow* window);
    void update(float deltaTime);
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
private:
    void updateVectors();

    glm::vec3 m_position = glm::vec3(0.0f);
    glm::vec3 m_front, m_right, m_up;

    const float m_speed = 2.0f;
    const float m_sensitivity = 0.001f;
    float m_yaw = glm::radians(-90.0f);
    float m_pitch = glm::radians(0.0f);
    float m_fov = glm::radians(45.0f);
    float m_aspect = 1.0f;

    const float m_pitchLimit = glm::radians(89.9f);

    glm::vec2 m_lastMousePos;
    bool m_capturingMouse = true;

    GLFWwindow* m_window = nullptr;
};
