#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/constants.hpp>

namespace Cory {
/// convert spherical coordinates (r, theta, phi) to cartesian (x, y, z)
inline glm::vec3 sphericalToCartesian(const glm::vec3 spherical);
/// convert cartesian (x, y, z) to spherical coordinates (r, theta, phi)
inline glm::vec3 cartesianToSpherical(const glm::vec3 cartesian);


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
inline glm::vec3 sphericalToCartesian(const glm::vec3 spherical)
{
    auto r = spherical.x;
    auto theta = spherical.y;
    auto phi = spherical.z;
    float x = r * sin(phi) * cos(theta);
    float y = r * sin(phi) * sin(theta);
    float z = r * cos(phi);
    return {x, y, z};
}

inline glm::vec3 cartesianToSpherical(const glm::vec3 cartesian)
{
    float r = glm::length(cartesian);
    if (cartesian.x == 0.0 && cartesian.y == 0.0) return {r, 0.0, 0.0};

    float theta = atan(cartesian.y / cartesian.x);
    float phi = atan(sqrt(cartesian.x * cartesian.x + cartesian.y * cartesian.y) / cartesian.z);

    if (cartesian.x < 0.0 && cartesian.y >= 0.0 && theta == 0.0) { theta = glm::pi<float>(); }
    else if (cartesian.x < 0.0 && cartesian.y < 0.0 && theta > 0.0) {
        theta -= glm::pi<float>();
    }
    else if (cartesian.x < 0.0 && cartesian.y > 0.0 && theta < 0.0) {
        theta += glm::pi<float>();
    }
    return {r, theta, phi};
}

}