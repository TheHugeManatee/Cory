#include <spdlog/spdlog.h>

#include <cstdlib>

#include "HelloTriangle.h"

int main()
{
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception &e) {
        spdlog::critical("Unhandled exception: {}", e.what());

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
