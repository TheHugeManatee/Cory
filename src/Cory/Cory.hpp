#pragma once

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
void Shutdown();

} // namespace Cory