#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <KDGpu/gpu_core.h>

namespace Cory::glmu {

// ~~~~~~~~~~~~~~~~~~ VEC2 ~~~~~~~~~~~~~~~~~~
namespace vec2 {
glm::vec2 from(auto someStruct)
{
    auto [x, y] = someStruct;
    return glm::vec2{x, y};
}

} // namespace vec2

// ~~~~~~~~~~~~~~~~~~ VEC3 ~~~~~~~~~~~~~~~~~~
namespace vec3 {
glm::vec3 from(auto someStruct)
{
    auto [x, y, z] = someStruct;
    return glm::vec3{x, y, z};
}

} // namespace vec3

// ~~~~~~~~~~~~~~~~~~ VEC4 ~~~~~~~~~~~~~~~~~~
namespace vec4 {
glm::vec4 from(auto someStruct)
{
    auto [x, y, z, w] = someStruct;
    return glm::vec4{x, y, z, w};
}

} // namespace vec4

// ~~~~~~~~~~~~~~~~~~ U32VEC2 ~~~~~~~~~~~~~~~~~~
namespace u32vec2 {
glm::u32vec2 from(auto someStruct)
{
    auto [x, y] = someStruct;
    return glm::u32vec2{x, y};
}
} // namespace u32vec2

// ~~~~~~~~~~~~~~~~~~ U32VEC3 ~~~~~~~~~~~~~~~~~~
namespace u32vec3 {
glm::u32vec3 from(auto someStruct)
{
    auto [x, y, z] = someStruct;
    return glm::u32vec3{x, y, z};
}

} // namespace u32vec3

// ~~~~~~~~~~~~~~~~~~ U32VEC4 ~~~~~~~~~~~~~~~~~~
namespace u32vec4 {
glm::u32vec4 from(auto someStruct)
{
    auto [x, y, z, w] = someStruct;
    return glm::u32vec4{x, y, z, w};
}

} // namespace u32vec4

template <typename T> T to(glm::vec2 v) { return T{v.x, v.y}; }
template <typename T> T to(glm::vec3 v) { return T{v.x, v.y, v.z}; }
template <typename T> T to(glm::vec4 v) { return T{v.x, v.y, v.z, v.w}; }
template <typename T> T to(glm::u32vec2 v) { return T{v.x, v.y}; }
template <typename T> T to(glm::u32vec3 v) { return T{v.x, v.y, v.z}; }
template <typename T> T to(glm::u32vec4 v) { return T{v.x, v.y, v.z, v.w}; }
template <typename T> T to(glm::i32vec2 v) { return T{v.x, v.y}; }
template <typename T> T to(glm::i32vec3 v) { return T{v.x, v.y, v.z}; }
template <typename T> T to(glm::i32vec4 v) { return T{v.x, v.y, v.z, v.w}; }

} // namespace Cory::glmu