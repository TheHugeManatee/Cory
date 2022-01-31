#include <spdlog/spdlog.h>

#include <cvk/Builder.h>
#include <cvk/FmtEnum.h>
#include <cvk/FmtStruct.h>

int main(int argc, char **argv)
{
    spdlog::info("Hellow");

    VkResult r{VK_ERROR_DEVICE_LOST};
    spdlog::error("Uh-oh: {}", r);

    VkImageCreateInfo s{};
    spdlog::info("Cool struct: {}", s);

    VkDevice device{};
    auto image = cvk::image_builder{device}
                     .extent({2, 3, 4})
                     .format(VK_FORMAT_B8G8R8_SRGB)
                     .queue_family_indices({1, 2, 3})
                     .mip_levels(1)
                     .create();


    spdlog::info("byeeeee");
    return 0;
}
