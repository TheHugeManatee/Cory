
#include "Cory/Log.h"

#include <cstdlib>

#include "VolumeRendering.h"

int main()
{
  VolumeRenderingApplication app;

  try {
    app.run();
  }
  catch (const std::exception &e) {
    CO_APP_FATAL("Unhandled exception: {}", e.what());

    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
