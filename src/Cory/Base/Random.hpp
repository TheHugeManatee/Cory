#pragma once

#include <random>

namespace Cory {

class RNG {
  public:
    static std::random_device &Device()
    {
        static std::random_device rd;
        return rd;
    }
    static std::mt19937 &Generator()
    {
        static std::mt19937 gen(Device()());
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
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        float theta = dis(Generator()) * glm::pi<float>();
        float phi = dis(Generator()) * glm::two_pi<float>();
        float r = std::cbrt(dis(Generator()));
        return glm::vec3{r * std::sin(theta) * std::cos(phi),
                         r * std::sin(theta) * std::sin(phi),
                         r * std::cos(theta)};
    }

    static glm::vec3 UniformDirection()
    {
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        float theta = dis(Generator()) * glm::two_pi<float>();
        float z = dis(Generator()) * 2.0f - 1.0f;

        return glm::vec3{sqrt(1 - z * z) * cos(theta), sqrt(1 - z * z) * sin(theta), z};
    }
};

} // namespace Cory