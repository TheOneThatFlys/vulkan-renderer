#pragma once

#include <bitset>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Common.h"

#define INPUT_MANAGER_MAX_CODES 162

class InputManager {
public:
    static void create(GLFWwindow* window);
    /// Needs to be called before `glfwPollEvents()`
    static void update();
    static void keyCallback(GLFWwindow*, int key, int, int action, int);
    static bool keyHeld(u32 key);
    static bool keyPressed(u32 key);
    static glm::vec2 mousePos();
    static glm::vec2 mouseMovement();
private:
    static u32 packCode(u32 code);
    static u32 unpackCode(u32 code);

    static GLFWwindow* m_window;
    static std::bitset<INPUT_MANAGER_MAX_CODES> m_heldKeys;
    static std::bitset<INPUT_MANAGER_MAX_CODES> m_pressedKeysThisFrame;
    static glm::vec2 m_currentMousePos;
    static glm::vec2 m_lastMousePos;
    static bool m_firstMouse;
};
