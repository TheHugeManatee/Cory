#include <Cory/Cory.hpp>

#include "ParticleComputeDemo.hpp"

#include <spdlog/spdlog.h>

#include <span>
#include <stdexcept>

int main(int argc, const char **argv)
{
    try {
        Cory::Init();
        ParticleComputeDemoApplication app{std::span{argv, size_t(argc)}};

        app.run();
    }
    catch (const std::exception &e) {
        spdlog::critical("Uncaught exception on main thread: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
