#pragma once

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <range/v3/range/concepts.hpp>

namespace Cory {
/// convert spherical coordinates (r, theta, phi) to cartesian (x, y, z)
inline glm::vec3 sphericalToCartesian(const glm::vec3 spherical);
/// convert cartesian (x, y, z) to spherical coordinates (r, theta, phi)
inline glm::vec3 cartesianToSpherical(const glm::vec3 cartesian);
/// this is the same as what boost::hash_combine does
template <typename T>
    requires(!ranges::range<T>)
std::size_t hashCombine(std::size_t seed, const T &v)
{
    return seed ^ std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
/// overload for ranges
template <typename Range>
    requires(ranges::range<Range>)
std::size_t hashCombine(std::size_t seed, Range &&rng)
{
    std::size_t hash = seed;
    for (const auto &v : rng) {
        hash = hashCombine(hash, v);
    }
    return hash;
}

/// variadic template to make composing hashes of different things easier
template <typename FirstArg> std::size_t hashCompose(std::size_t seed, FirstArg &&arg)
{
    return hashCombine(seed, std::forward<FirstArg>(arg));
}
template <typename FirstArg, typename... Args>
std::size_t hashCompose(std::size_t seed, FirstArg &&arg, Args &&...args)
{
    return hashCompose(hashCombine(seed, std::forward<FirstArg>(arg)), std::forward<Args>(args)...);
}

// Matrix corresponds to Translate * Ry * Rx * Rz * Scale
// Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
// https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
inline glm::mat4 makeTransform(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale)
{
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    return glm::mat4{{
                         scale.x * (c1 * c3 + s1 * s2 * s3),
                         scale.x * (c2 * s3),
                         scale.x * (c1 * s2 * s3 - c3 * s1),
                         0.0f,
                     },
                     {
                         scale.y * (c3 * s1 * s2 - c1 * s3),
                         scale.y * (c2 * c3),
                         scale.y * (c1 * c3 * s2 + s1 * s3),
                         0.0f,
                     },
                     {
                         scale.z * (c2 * s1),
                         scale.z * (-s2),
                         scale.z * (c1 * c2),
                         0.0f,
                     },
                     {translation.x, translation.y, translation.z, 1.0f}};
}

inline glm::mat4 makeOrtho(float left, float right, float top, float bottom, float near, float far)
{
    glm::mat4 ret{1.0f};
    ret[0][0] = 2.f / (right - left);
    ret[1][1] = 2.f / (bottom - top);
    ret[2][2] = 1.f / (far - near);
    ret[3][0] = -(right + left) / (right - left);
    ret[3][1] = -(bottom + top) / (bottom - top);
    ret[3][2] = -near / (far - near);
    return ret;
}

inline glm::mat4 makePerspective(float fovy, float aspect, float near, float far)
{
    assert(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);
    const float tanHalfFovy = tan(fovy / 2.f);
    glm::mat4 ret{1.0f};
    ret = glm::mat4{0.0f};
    ret[0][0] = 1.f / (aspect * tanHalfFovy);
    ret[1][1] = 1.f / (tanHalfFovy);
    ret[2][2] = far / (far - near);
    ret[2][3] = 1.f;
    ret[3][2] = -(far * near) / (far - near);
    return ret;
}

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

} // namespace Cory