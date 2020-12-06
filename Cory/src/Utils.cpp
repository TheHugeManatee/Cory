#include "Utils.h"

#include <fmt/format.h>

#include <array>
#include <cassert>
#include <stdexcept>
#include <string_view>
#include <fstream>

namespace Cory {
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

std::vector<char> readFile(const std::filesystem::path &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("failed to open file {}", filename.string()));
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

stbi_image::stbi_image(const std::string &file)
{
    data = stbi_load(file.c_str(), &width, &height, &channels, STBI_rgb_alpha);
}

stbi_image::~stbi_image() { stbi_image_free(data); }

} // namespace Cory