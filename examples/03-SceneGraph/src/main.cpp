#include <Cory/Cory.hpp>

#include "SceneGraphDemo.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <span>
#include <stdexcept>

int main(int argc, const char **argv)
{
    try {
        Cory::Init();
        SceneGraphDemoApplication app{std::span{argv, size_t(argc)}};

        app.run();
    }
    catch (const std::exception &e) {
        spdlog::critical("Uncaught exception on main thread: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}