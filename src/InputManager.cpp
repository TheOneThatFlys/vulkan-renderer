#include "InputManager.h"

#include "AssetManager.h"

InputManager InputManager::s_instance{};

void InputManager::setWindow(GLFWwindow *window) {
    get()->m_window = window;
    glfwSetKeyCallback(window, keyCallback);
}

void InputManager::update(float) {
    get()->m_pressedKeysThisFrame.reset();
    get()->m_lastMousePos = get()->m_currentMousePos;
    double x, y;
    glfwGetCursorPos(get()->m_window, &x, &y);
    get()->m_currentMousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
    if (get()->m_firstMouse) {
        get()->m_lastMousePos = get()->m_currentMousePos;
        get()->m_firstMouse = false;
    }
}

void InputManager::keyCallback(GLFWwindow *, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        const u32 packed = packCode(key);
        get()->m_heldKeys.set(packed);
        get()->m_pressedKeysThisFrame.set(packed);
    } else if (action == GLFW_RELEASE) {
        get()->m_heldKeys.reset(packCode(key));
    }
}

bool InputManager::keyHeld(const u32 key) {
    return get()->m_heldKeys.test(packCode(key));
}

bool InputManager::keyPressed(const u32 key) {
    return get()->m_pressedKeysThisFrame.test(packCode(key));
}

glm::vec2 InputManager::mousePos() {
    return get()->m_currentMousePos;
}

glm::vec2 InputManager::mouseMovement() {
    return get()->m_lastMousePos - get()->m_currentMousePos;
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

InputManager * InputManager::get() {
    return &s_instance;
}
