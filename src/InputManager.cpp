#include "InputManager.h"

#include "AssetManager.h"

InputManager InputManager::s_instance{};

void InputManager::setWindow(GLFWwindow *window) {
    get().m_window = window;
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, mouseScrollCallback);
}

void InputManager::update() {
    get().m_pressedKeysThisFrame.reset();
    get().m_lastMousePos = get().m_currentMousePos;
    get().m_mouseScroll = glm::vec2(0.0f);
    double x, y;
    glfwGetCursorPos(get().m_window, &x, &y);
    get().m_currentMousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
    if (get().m_firstMouse) {
        get().m_lastMousePos = get().m_currentMousePos;
        get().m_firstMouse = false;
    }
}

bool InputManager::keyHeld(const u32 key) {
    return get().m_heldKeys.test(packCode(key));
}

bool InputManager::keyPressed(const u32 key) {
    return get().m_pressedKeysThisFrame.test(packCode(key));
}

bool InputManager::mouseHeld(u32 button) {
    return glfwGetMouseButton(get().m_window, button) == GLFW_PRESS;
}

glm::vec2 InputManager::mousePos() {
    return get().m_currentMousePos;
}

glm::vec2 InputManager::mouseMovement() {
    return get().m_lastMousePos - get().m_currentMousePos;
}

glm::vec2 InputManager::mouseScroll() {
    return get().m_mouseScroll;
}

void InputManager::disableMouseAcceleration() {
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(get().m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    } else {
        Logger::warn("Raw mouse input mode not supported");
    }
}

u32 InputManager::packCode(u32 code) {
    if (code > 162) {
        code -= 159;
    }
    return code - 32;
}

u32 InputManager::unpackCode(u32 code) {
    code += 32;
    if (code > 96) {
        code += 159;
    }
    return code;
}

InputManager & InputManager::get() {
    return s_instance;
}

void InputManager::keyCallback(GLFWwindow *, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        const u32 packed = packCode(key);
        get().m_heldKeys.set(packed);
        get().m_pressedKeysThisFrame.set(packed);
    } else if (action == GLFW_RELEASE) {
        get().m_heldKeys.reset(packCode(key));
    }
}

void InputManager::mouseScrollCallback(GLFWwindow *, double dx, double dy) {
    get().m_mouseScroll += glm::vec2(dx, dy);
}
