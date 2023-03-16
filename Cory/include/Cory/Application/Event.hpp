#pragma once

#include <Cory/Base/BitField.hpp>

#include <glm/vec2.hpp>

#include <stdint.h>
#include <variant>

namespace Cory {

enum class MouseButton { None, Left, Middle, Right };
enum class ModifierFlagBits : uint32_t { Shift = 1, Ctrl = 2, Alt = 4, Super = 8};
using ModifierFlags = BitField<ModifierFlagBits>;
enum class ButtonAction { None, Release, Press, Repeat };

struct SwapchainResizedEvent {
    glm::i32vec2 size;
};

struct MouseMovedEvent {
    glm::vec2 position;
    MouseButton button;
    ModifierFlags modifiers;
};

struct MouseButtonEvent {
    glm::vec2 position;
    MouseButton button;
    ButtonAction action;
    ModifierFlags modifiers;
};

struct ScrollEvent {
    glm::vec2 position;
    glm::vec2 scrollDelta;
};

struct KeyEvent {
    int key;
    int scanCode;
    int action;
    int modifiers;
};

using Event =
    std::variant<SwapchainResizedEvent, MouseMovedEvent, MouseButtonEvent, ScrollEvent, KeyEvent>;

} // namespace Cory