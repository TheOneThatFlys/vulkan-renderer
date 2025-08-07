#include "InputManager.h"

GLFWwindow* InputManager::m_window = nullptr;
std::bitset<INPUT_MANAGER_MAX_CODES> InputManager::m_heldKeys = std::bitset<INPUT_MANAGER_MAX_CODES>();
std::bitset<INPUT_MANAGER_MAX_CODES> InputManager::m_pressedKeysThisFrame = std::bitset<INPUT_MANAGER_MAX_CODES>();
glm::vec2 InputManager::m_currentMousePos = glm::vec2(0.0f);
glm::vec2 InputManager::m_lastMousePos = glm::vec2(-1.0f);
bool InputManager::m_firstMouse = true;

void InputManager::create(GLFWwindow *window) {
    m_window = window;
    glfwSetKeyCallback(window, keyCallback);
}

void InputManager::update() {
    m_pressedKeysThisFrame.reset();

    m_lastMousePos = m_currentMousePos;
    double x, y;
    glfwGetCursorPos(m_window, &x, &y);
    m_currentMousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
    if (m_firstMouse) {
        m_lastMousePos = m_currentMousePos;
        m_firstMouse = false;
    }
}

void InputManager::keyCallback(GLFWwindow *, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        const u32 packed = packCode(key);
        m_heldKeys.set(packed);
        m_pressedKeysThisFrame.set(packed);
    } else if (action == GLFW_RELEASE) {
        m_heldKeys.reset(packCode(key));
    }
}

bool InputManager::keyHeld(const u32 key) {
    return m_heldKeys.test(packCode(key));
}

bool InputManager::keyPressed(const u32 key) {
    return m_pressedKeysThisFrame.test(packCode(key));
}

glm::vec2 InputManager::mousePos() {
    return m_currentMousePos;
}

glm::vec2 InputManager::mouseMovement() {
    return m_lastMousePos - m_currentMousePos;
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
