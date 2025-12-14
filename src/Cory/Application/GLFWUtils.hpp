#include <Cory/Application/Event.hpp>

#include <GLFW/glfw3.h>

namespace Cory::GLFWUtils {
inline [[nodiscard]] MouseButton getMouseButtonState(GLFWwindow *window)
{
    const MouseButton mouseButton =
        (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) ? Cory::MouseButton::Left
        : (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) ? MouseButton::Middle
        : (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)  ? MouseButton::Right
                                                                               : MouseButton::None;
    return mouseButton;
}

inline [[nodiscard]] ModifierFlags getModifierState(GLFWwindow *window)
{
    ModifierFlags modifiers;
    if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
        modifiers.set(ModifierFlagBits::Alt);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        modifiers.set(ModifierFlagBits::Ctrl);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        modifiers.set(ModifierFlagBits::Shift);
    }
    return modifiers;
}
} // namespace Cory::GLFWUtils