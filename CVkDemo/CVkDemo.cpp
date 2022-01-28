#include <spdlog/spdlog.h>

#include <cvk/FmtEnum.h>
#include <cvk/FmtStruct.h>

int main(int argc, char** argv) {
    spdlog::info("Hellow");

    VkResult r{VK_ERROR_DEVICE_LOST};
    spdlog::error("Uh-oh: {}", r);

    VkImageCreateInfo s{};
    spdlog::info("Cool struct: {}", s);

    return 0;
}
