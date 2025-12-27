#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/constants.hpp>

#include <random>

namespace Cory {

class RNG {
  public:
    static std::random_device &Device()
    {
        thread_local std::random_device rd;
        return rd;
    }
    static std::mt19937 &Generator()
    {
        thread_local std::mt19937 gen(Device()());
        return gen;
    }

    template <typename T> static T Uniform(T min, T max)
    {
        if constexpr (std::is_floating_point_v<T>) {
            std::uniform_real_distribution<T> dis(min, max);
            return dis(Generator());
        }
        else {
            std::uniform_int_distribution<T> dis(min, max);
            return dis(Generator());
        }
    }

    static glm::vec3 UniformInSphere()
    {
        thread_local std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        // Uniform volume: sample z uniformly in [-1, 1], phi uniformly in [0, 2pi).
        float z = dis(Generator()) * 2.0f - 1.0f;
        float phi = dis(Generator()) * glm::two_pi<float>();
        float r = std::cbrt(dis(Generator()));
        float xy = std::sqrt(std::max(0.0f, 1.0f - z * z));
        return glm::vec3{r * xy * std::cos(phi), r * xy * std::sin(phi), r * z};
    }

    static glm::vec3 UniformDirection()
    {
        thread_local std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        float theta = dis(Generator()) * glm::two_pi<float>();
        float z = dis(Generator()) * 2.0f - 1.0f;

        const float xy = std::sqrt(std::max(0.0f, 1.0f - z * z));
        return glm::vec3{xy * std::cos(theta), xy * std::sin(theta), z};
    }
};

} // namespace Cory