#include <spdlog/spdlog.h>

#include <cvk/FmtEnum.h>

int main(int argc, char** argv) {
    spdlog::info("Hellow");

    VkResult r{VK_ERROR_DEVICE_LOST};
    spdlog::error("Uh-oh: {}", r);

    return 0;
}
