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
    size_t r8ByteSize{};
};

struct BmpImage {
    uint32_t width{};
    uint32_t height{};
    std::vector<std::byte> pixelsR8{};
};

struct BmpImageRgba8 {
    uint32_t width{};
    uint32_t height{};
    std::vector<std::byte> pixelsRgba8{};
};

[[nodiscard]] Result<BmpInfo> queryBmpInfo(std::span<const std::byte> bytes);
[[nodiscard]] Result<BmpInfo> decodeBmp(std::span<const std::byte> bytes,
                                        std::span<std::byte> outputR8);
[[nodiscard]] Result<BmpImage> loadBmp(const std::filesystem::path &path);

[[nodiscard]] Result<BmpImageRgba8> decodeBmpRgba8(std::span<const std::byte> bytes);
[[nodiscard]] Result<BmpImageRgba8> loadBmpRgba8(const std::filesystem::path &path);
Result<void> writeBmpRgba8(const std::filesystem::path &path, const BmpImageRgba8 &image);

} // namespace Cory::IO
