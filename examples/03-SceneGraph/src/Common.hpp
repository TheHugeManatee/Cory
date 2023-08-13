#pragma once

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

struct AnimationComponent {
    glm::vec4 color{1.0, 0.0, 0.0, 1.0};
    float blend;
    float entityIndex{};
};

struct CameraComponent {
    glm::mat4 viewMatrix{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f};
    float fovy{glm::radians(45.0f)};
    float nearPlane{0.1f};
    float farPlane{100.0f};
};
