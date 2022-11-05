#include "CubeDemo.hpp"

#include <cstdlib>
#include <stdexcept>

#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
    try {
        CubeDemoApplication app{argc, argv};

        app.run();
    }
    catch (const std::exception &e) {
        spdlog::critical("Uncaught exception on main thread: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}