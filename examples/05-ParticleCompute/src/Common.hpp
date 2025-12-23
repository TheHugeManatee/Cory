#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct PointSpriteComponent {
    glm::vec3 position{0.0f};
    float radius{0.1f};
    glm::vec4 color{1.0, 0.0, 0.0, 1.0};
};
