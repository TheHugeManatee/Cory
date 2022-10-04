#pragma once

#include <string>

namespace Cory {
    /**
     * Initialize the library.
     *
     * Mostly takes care of initializing all static (global) objects.
     *
     */
    void Init();
    std::string queryVulkanInstanceVersion();

    // temporary main function for prototyping
    void playground_main();
}