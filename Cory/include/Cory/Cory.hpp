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

    /**
     * Deinitialize the library.
     *
     * Mostly takes care of deinitializing all static (global) objects.
     */
    void Deinit();

    std::string queryVulkanInstanceVersion();

    // dumps a bunch of information onto the console
    void dumpInstanceInformation();
}