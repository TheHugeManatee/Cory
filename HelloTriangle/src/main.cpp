
#include "Cory/Log.h"

#include <cstdlib>

#include "HelloTriangle.h"

int main()
{
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception &e) {
        CO_APP_FATAL("Unhandled exception: {}", e.what());

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
