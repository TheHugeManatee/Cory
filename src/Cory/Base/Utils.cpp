#include <Cory/Base/Utils.hpp>

#include <fmt/format.h>

#include <array>
#include <fstream>
#include <stdexcept>

namespace Cory {
std::string formatBytes(size_t bytes)
{
    const static std::array suffix{"B", "KiB", "MiB", "GiB", "TiB"};
    uint32_t suff{};
    size_t remaind{};
    for (; suff < suffix.size() && bytes >= 1024; ++suff) {
        remaind = bytes % 1024;
        bytes /= 1024;
    }

    if (remaind == 0) return fmt::format("{} {}", bytes, suffix[suff]);

    return fmt::format("{:.2f} {}", float(bytes) + float(remaind) / 1024.f, suffix[suff]);
}

std::vector<char> readFile(const std::filesystem::path &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("failed to open file {}", filename.string()));
    }
    const auto position = file.tellg();
    if (position < 0) {
        throw std::runtime_error(fmt::format("failed to read file {}", filename.string()));
    }
    const std::streamsize fileSize = position;
    std::vector<char> buffer(static_cast<std::size_t>(fileSize));
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

stbi_image::stbi_image(const std::string &file)
{
    data = stbi_load(file.c_str(), &width, &height, &channels, STBI_rgb_alpha);
}

stbi_image::~stbi_image()
{
    stbi_image_free(data);
}

} // namespace Cory
