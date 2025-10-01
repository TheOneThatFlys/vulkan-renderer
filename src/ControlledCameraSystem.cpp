#include "ControlledCameraSystem.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "VulkanEngine.h"
#include "Renderer3D.h"
#include "Components.h"
#include "InputManager.h"

ControlledCameraSystem::ControlledCameraSystem() {
    // capture mouse
    glfwSetInputMode(VulkanEngine::getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    InputManager::disableMouseAcceleration();
}

void ControlledCameraSystem::update(float deltaTime) {
    // movement
    ControlledCamera& camera = getCamera();

    float multiplier = deltaTime * camera.speed;
    const glm::vec3 forwards = glm::normalize(glm::vec3(cos(camera.yaw), 0.0f, sin(camera.yaw)));
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
        camera.position += glm::normalize(dv) * multiplier;
    }

    // escaping capture
    if (InputManager::keyPressed(GLFW_KEY_ESCAPE)) {
        camera.capturingMouse = !camera.capturingMouse;
        if (camera.capturingMouse) {
            glfwSetInputMode(VulkanEngine::getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
            VulkanEngine::getRenderer()->getModelSelector()->disable();

        } else {
            glfwSetInputMode(VulkanEngine::getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            VulkanEngine::getRenderer()->getModelSelector()->enable();
        }
    }

    // skip mouse input if not capturing mouse
    if (!camera.capturingMouse) return;
    // mouse movement
    const glm::vec2 mouseMovement = InputManager::mouseMovement();
    if (!(mouseMovement.x == 0 && mouseMovement.y == 0)) {
        camera.yaw -= mouseMovement.x * camera.sensitivity;
        camera.pitch += mouseMovement.y * camera.sensitivity;

        if (camera.pitch > glm::radians(89.9f)) camera.pitch = glm::radians(89.9f);
        if (camera.pitch < glm::radians(-89.9f)) camera.pitch = glm::radians(-89.9f);

        if (camera.yaw > glm::radians(180.0f)) camera.yaw -= glm::radians(360.0f);
        if (camera.yaw < glm::radians(-180.0f)) camera.yaw += glm::radians(360.0f);
    }

    camera.fov += InputManager::mouseScroll().y * -multiplier;
    if (camera.fov < 0.0f) camera.fov = 0.0f;
    if (camera.fov > glm::radians(180.0f)) camera.fov = glm::radians(180.0f);
}

glm::mat4 ControlledCameraSystem::getViewMatrix() const {
    const ControlledCamera& camera = getCamera();
    const auto [front, right, up] = getVectors();
    return glm::lookAt(camera.position, camera.position + front, up);
}

glm::mat4 ControlledCameraSystem::getProjectionMatrix() const {
    const ControlledCamera& camera = getCamera();
    glm::mat4 temp = glm::perspective(camera.fov, camera.aspect, camera.near, camera.far);
    temp[1][1] *= -1; // glm assumes opengl coordinates so we flip for vulkan
    return temp;
}

Frustum ControlledCameraSystem::getFrustum() const {
    const ControlledCamera& camera = getCamera();
    const auto [cameraFront, cameraRight, cameraUp] = getVectors();

    const float halfHeight = camera.far * std::tan(camera.fov * 0.5f);
    const float halfWidth = halfHeight * camera.aspect;
    const glm::vec3 frontMulFar = camera.far * cameraFront;

    return {
        .top = {glm::normalize(glm::cross(cameraRight, frontMulFar - cameraUp * halfHeight)), camera.position},
        .bottom = {glm::normalize(glm::cross(frontMulFar + cameraUp * halfHeight, cameraRight)), camera.position},
        .right = {glm::normalize(glm::cross(frontMulFar - cameraRight * halfWidth, cameraUp)), camera.position},
        .left = {glm::normalize(glm::cross(cameraUp, frontMulFar + cameraRight * halfWidth)), camera.position},
        .near = {cameraFront, camera.position + cameraFront * camera.near},
        .far = {-cameraFront, camera.position + cameraFront * (camera.near + camera.far)}
    };
}

glm::vec3 ControlledCameraSystem::getFrontVector() const {
    const ControlledCamera& camera = getCamera();
    return glm::normalize(glm::vec3(
        cos(camera.yaw) * cos(camera.pitch),
        sin(camera.pitch),
        sin(camera.yaw) * cos(camera.pitch)
    ));
}

CameraVectors ControlledCameraSystem::getVectors() const {
    const glm::vec3 front = getFrontVector();
    const glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 up = glm::normalize(glm::cross(right, front));

    return {
        .front = front,
        .right = right,
        .up = up
    };
}

Ray ControlledCameraSystem::normalisedScreenToRay(const glm::vec2 normalisedScreenCoordinates) const {
    const ControlledCamera& camera = getCamera();
    return {
        .origin = camera.position,
        .direction = glm::normalize(glm::inverse(glm::mat3(getViewMatrix())) * glm::vec3(glm::vec2(glm::inverse(getProjectionMatrix()) * glm::vec4(normalisedScreenCoordinates.x, normalisedScreenCoordinates.y, -1.0f, 1.0f)), -1.0f))
    };
}

ControlledCamera & ControlledCameraSystem::getCamera() const {
    assert(!m_entities.empty() && "Could not find camera entity");
    return ECS::getComponent<ControlledCamera>(*m_entities.begin());
}
