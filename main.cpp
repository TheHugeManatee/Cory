#include <spdlog/spdlog.h>

#include <cstdlib>
#include <stdexcept>

#include "HelloTriangle.h"

int main()
{
	HelloTriangleApplication app;

	try {
		app.run();
	}
	catch (const std::exception &e) {
		spdlog::error("Unhandled exception: {}", e.what());

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
