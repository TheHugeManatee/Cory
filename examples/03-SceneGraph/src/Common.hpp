#pragma once

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

struct AnimationComponent {
    glm::vec4 color{1.0, 0.0, 0.0, 1.0};
    float blend;
    float entityIndex{};
};
