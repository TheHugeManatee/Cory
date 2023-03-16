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

    template<typename T> static T Uniform(T min, T max)
    {
        std::uniform_real_distribution<T> dis(min, max);
        return dis(Generator());
    }

};

} // namespace Cory