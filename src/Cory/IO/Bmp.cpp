#include <Cory/IO/Bmp.hpp>

#include <fstream>
#include <limits>

namespace Cory::IO {
namespace {

[[nodiscard]] auto readU16(std::span<const std::byte> bytes, size_t offset) -> uint16_t
{
    return uint16_t(static_cast<uint8_t>(bytes[offset])) |
           (uint16_t(static_cast<uint8_t>(bytes[offset + 1])) << 8U);
}

[[nodiscard]] auto readU32(std::span<const std::byte> bytes, size_t offset) -> uint32_t
{
    return uint32_t(static_cast<uint8_t>(bytes[offset])) |
           (uint32_t(static_cast<uint8_t>(bytes[offset + 1])) << 8U) |
           (uint32_t(static_cast<uint8_t>(bytes[offset + 2])) << 16U) |
           (uint32_t(static_cast<uint8_t>(bytes[offset + 3])) << 24U);
}

[[nodiscard]] auto readI32(std::span<const std::byte> bytes, size_t offset) -> int32_t
{
    return static_cast<int32_t>(readU32(bytes, offset));
}

[[nodiscard]] auto fail(const char *message) -> std::unexpected<std::string>
{
    return std::unexpected(std::string{message});
}

} // namespace

Result<BmpImage> decodeBmp(std::span<const std::byte> bytes)
{
    constexpr size_t fileHeaderSize = 14;
    constexpr size_t dibHeaderMinSize = 40;
    constexpr size_t minBmpSize = fileHeaderSize + dibHeaderMinSize;

    if (bytes.size() < minBmpSize) return fail("BMP decode failed: file is too small");

    if (static_cast<uint8_t>(bytes[0]) != 'B' || static_cast<uint8_t>(bytes[1]) != 'M') {
        return fail("BMP decode failed: invalid signature");
    }

    const auto pixelDataOffset = readU32(bytes, 10);
    const auto dibHeaderSize = readU32(bytes, 14);
    if (dibHeaderSize < dibHeaderMinSize) {
        return fail("BMP decode failed: unsupported DIB header");
    }

    const auto requiredHeaderBytes = fileHeaderSize + static_cast<size_t>(dibHeaderSize);
    if (bytes.size() < requiredHeaderBytes) {
        return fail("BMP decode failed: truncated header");
    }

    const auto widthSigned = readI32(bytes, 18);
    const auto heightSigned = readI32(bytes, 22);
    const auto planes = readU16(bytes, 26);
    const auto bitCount = readU16(bytes, 28);
    const auto compression = readU32(bytes, 30);

    if (widthSigned <= 0) return fail("BMP decode failed: invalid width");
    if (heightSigned == 0) return fail("BMP decode failed: invalid height");
    if (heightSigned == std::numeric_limits<int32_t>::min()) {
        return fail("BMP decode failed: invalid height");
    }
    if (planes != 1) return fail("BMP decode failed: invalid planes");
    if (compression != 0) return fail("BMP decode failed: compressed BMP is unsupported");
    if (bitCount != 24 && bitCount != 32) {
        return fail("BMP decode failed: only 24-bit and 32-bit BMP are supported");
    }

    const auto width = static_cast<uint32_t>(widthSigned);
    const auto topDown = heightSigned < 0;
    const auto height = static_cast<uint32_t>(topDown ? -heightSigned : heightSigned);
    const auto bytesPerPixel = static_cast<uint64_t>(bitCount / 8);
    const auto rowSizeRaw = static_cast<uint64_t>(width) * bytesPerPixel;
    const auto rowStride = (rowSizeRaw + 3ULL) & ~3ULL;
    const auto pixelArraySize = rowStride * static_cast<uint64_t>(height);

    if (pixelDataOffset > bytes.size()) {
        return fail("BMP decode failed: invalid pixel data offset");
    }

    if (pixelArraySize > std::numeric_limits<size_t>::max() ||
        static_cast<size_t>(pixelArraySize) > bytes.size() - pixelDataOffset) {
        return fail("BMP decode failed: truncated pixel data");
    }

    const auto pixelCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    if (pixelCount > std::numeric_limits<size_t>::max() / 4ULL) {
        return fail("BMP decode failed: image is too large");
    }

    BmpImage image{};
    image.width = width;
    image.height = height;
    image.pixelsRgba8.resize(static_cast<size_t>(pixelCount) * 4U);

    const auto *src = reinterpret_cast<const uint8_t *>(bytes.data());
    auto *dst = reinterpret_cast<uint8_t *>(image.pixelsRgba8.data());
    for (uint32_t y = 0; y < height; ++y) {
        const auto srcY = topDown ? y : (height - 1U - y);
        const auto srcRowOffset = static_cast<size_t>(pixelDataOffset) +
                                  static_cast<size_t>(srcY * rowStride);
        const auto dstRowOffset = static_cast<size_t>(y) * static_cast<size_t>(width) * 4U;

        for (uint32_t x = 0; x < width; ++x) {
            const auto srcOffset = srcRowOffset + static_cast<size_t>(x) * static_cast<size_t>(bytesPerPixel);
            const auto dstOffset = dstRowOffset + static_cast<size_t>(x) * 4U;
            dst[dstOffset + 0] = src[srcOffset + 2];
            dst[dstOffset + 1] = src[srcOffset + 1];
            dst[dstOffset + 2] = src[srcOffset + 0];
            dst[dstOffset + 3] = (bytesPerPixel == 4) ? src[srcOffset + 3] : 255U;
        }
    }

    return image;
}

Result<BmpImage> loadBmp(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::unexpected("BMP load failed: could not open file '" + path.string() + "'");
    }

    const auto end = file.tellg();
    if (end < 0) {
        return std::unexpected("BMP load failed: failed to read size of '" + path.string() + "'");
    }

    std::vector<std::byte> bytes(static_cast<size_t>(end));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), end)) {
        return std::unexpected("BMP load failed: failed to read file '" + path.string() + "'");
    }

    return decodeBmp(bytes);
}

} // namespace Cory::IO
