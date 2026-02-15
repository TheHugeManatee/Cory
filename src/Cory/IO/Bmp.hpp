#pragma once

#include <Cory/Base/Result.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace Cory::IO {

struct BmpInfo {
    uint32_t width{};
    uint32_t height{};
    size_t rgba8ByteSize{};
};

struct BmpImage {
    uint32_t width{};
    uint32_t height{};
    std::vector<std::byte> pixelsRgba8{};
};

[[nodiscard]] Result<BmpInfo> queryBmpInfo(std::span<const std::byte> bytes);
[[nodiscard]] Result<BmpInfo> decodeBmp(std::span<const std::byte> bytes, std::span<std::byte> outputRgba8);
[[nodiscard]] Result<BmpImage> loadBmp(const std::filesystem::path &path);

} // namespace Cory::IO
