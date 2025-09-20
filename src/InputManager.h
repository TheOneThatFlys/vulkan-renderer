#pragma once

#include <bitset>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Common.h"

constexpr u32 INPUT_MANAGER_MAX_CODES = 162;

class InputManager {
public:
    static void setWindow(GLFWwindow* window);
    static bool keyHeld(u32 key);
    static bool keyPressed(u32 key);
    static bool mouseHeld(u32 button);
    static glm::vec2 mousePos();
    static glm::vec2 mouseMovement();
    static glm::vec2 mouseScroll();
    static void disableMouseAcceleration();
    static u32 packCode(u32 code);
    static u32 unpackCode(u32 code);
    static InputManager& get();

    /// Needs to be called before `glfwPollEvents()`
    static void update();
private:
    explicit InputManager() = default;
    static void keyCallback(GLFWwindow*, int key, int, int action, int);
    static void mouseScrollCallback(GLFWwindow*, double dx, double dy);

    static InputManager s_instance;

    GLFWwindow* m_window = nullptr;
    std::bitset<INPUT_MANAGER_MAX_CODES> m_heldKeys;
    std::bitset<INPUT_MANAGER_MAX_CODES> m_pressedKeysThisFrame;
    glm::vec2 m_currentMousePos = glm::vec2(0.0f);
    glm::vec2 m_lastMousePos = glm::vec2(-1.0f);
    glm::vec2 m_mouseScroll = glm::vec2(0.0f);
    bool m_firstMouse = true;
};
