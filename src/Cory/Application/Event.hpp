#pragma once

#include <Cory/Base/BitField.hpp>
#include <Cory/Base/Primitives.hpp>

#include <glm/vec2.hpp>

#include <stdint.h>
#include <variant>

namespace Cory {

enum class MouseButton { None, Left, Middle, Right };
enum class ModifierFlagBits : uint32_t { Shift = 1, Ctrl = 2, Alt = 4, Super = 8 };
using ModifierFlags = BitField<ModifierFlagBits>;
enum class ButtonAction { None, Release, Press, Repeat };

struct SwapchainResizedEvent {
    i32vec2 size;
};

struct MouseMovedEvent {
    f32vec2 position;
    MouseButton button;
    ModifierFlags modifiers;
};

struct MouseButtonEvent {
    f32vec2 position;
    MouseButton button;
    ButtonAction action;
    ModifierFlags modifiers;
};

struct ScrollEvent {
    f32vec2 position;
    f32vec2 scrollDelta;
    ModifierFlags modifiers;
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
