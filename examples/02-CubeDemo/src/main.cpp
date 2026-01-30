#include <Cory/Cory.hpp>

#include "CubeDemo.hpp"

#include <stdexcept>

#include <gsl/gsl>
#include <spdlog/spdlog.h>

#include <span>

int main(int argc, const char **argv)
{
    try {
        Cory::Init();
        CubeDemoApplication app{argc, argv};

        app.run();
    }
    catch (const std::exception &e) {
        spdlog::critical("Uncaught exception on main thread: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
