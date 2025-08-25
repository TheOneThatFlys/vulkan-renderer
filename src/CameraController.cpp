#include "CameraController.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "InputManager.h"

CameraController::CameraController(GLFWwindow* window) : m_window(window) {
    updateVectors();
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    m_aspect = static_cast<float>(width) / static_cast<float>(height);
    // capture mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void CameraController::update(float deltaTime) {
    // movement
    float multiplier = deltaTime * speed;
    const glm::vec3 forwards = glm::normalize(glm::vec3(cos(m_yaw), 0.0f, sin(m_yaw)));
    constexpr glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(forwards, up));
    glm::vec3 dv = { 0.0f, 0.0f, 0.0f };
    if (InputManager::keyHeld(GLFW_KEY_LEFT_CONTROL)) {
        multiplier *= 3.0f;
    }
    if (InputManager::keyHeld(GLFW_KEY_W)) {
        dv += forwards;
    }
    if (InputManager::keyHeld(GLFW_KEY_S)) {
        dv -= forwards;
    }
    if (InputManager::keyHeld(GLFW_KEY_A)) {
        dv -= right;
    }
    if (InputManager::keyHeld(GLFW_KEY_D)) {
        dv += right;
    }
    if (InputManager::keyHeld(GLFW_KEY_SPACE)) {
        dv += up;
    }
    if (InputManager::keyHeld(GLFW_KEY_LEFT_SHIFT)) {
        dv -= up;
    }

    if (glm::length(dv) != 0.0f) {
        position += glm::normalize(dv) * multiplier;
    }

    // escaping capture
    if (InputManager::keyPressed(GLFW_KEY_ESCAPE)) {
        m_capturingMouse = !m_capturingMouse;
        if (m_capturingMouse) {
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
        } else {
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        }
    }

    // skip mouse input if not capturing mouse
    if (!m_capturingMouse) return;
    // mouse movement
    const glm::vec2 mouseMovement = InputManager::mouseMovement();
    if (!(mouseMovement.x == 0 && mouseMovement.y == 0)) {
        m_yaw -= mouseMovement.x * m_sensitivity;
        m_pitch += mouseMovement.y * m_sensitivity;

        if (m_pitch > m_pitchLimit) m_pitch = m_pitchLimit;
        if (m_pitch < -m_pitchLimit) m_pitch = -m_pitchLimit;

        if (m_yaw > glm::radians(180.0f)) m_yaw -= glm::radians(360.0f);
        if (m_yaw < glm::radians(-180.0f)) m_yaw += glm::radians(360.0f);

        updateVectors();
    }
}

glm::mat4 CameraController::getViewMatrix() const {
    return glm::lookAt(position, position + m_front, m_up);
}

glm::mat4 CameraController::getProjectionMatrix() const {
    glm::mat4 temp = glm::perspective(fov, m_aspect, 0.1f, 100.0f);
    temp[1][1] *= -1; // glm assumes opengl coordinates so we flip for vulkan
    return temp;
}

void CameraController::updateVectors() {
    m_front = glm::normalize(glm::vec3(
        cos(m_yaw) * cos(m_pitch),
        sin(m_pitch),
        sin(m_yaw) * cos(m_pitch)
    ));
    m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}
