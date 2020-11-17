
#include "Utils.h"

#include <cassert>
#include <fmt/format.h>
#include <stdexcept>

#include <spdlog/spdlog.h>

std::string formatBytes(size_t bytes)
{
    const static std::array<std::string_view, 5> suffix{"b", "KiB", "MiB", "GiB", "TiB"};
    uint32_t suff{};
    size_t remaind{};
    for (; suff < suffix.size() && bytes >= 1024; ++suff) {
        remaind = bytes % 1024;
        bytes /= 1024;
    }

    if (remaind == 0)
        return fmt::format("{} {}", bytes, suffix[suff]);

    return fmt::format("{:.2f} {}", float(bytes) + float(remaind) / 1024.f, suffix[suff]);
}

stbi_image::stbi_image(const std::string &file)
{
    data = stbi_load(file.c_str(), &width, &height, &channels, STBI_rgb_alpha);
}

stbi_image::~stbi_image() { stbi_image_free(data); }
