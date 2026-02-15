#include <Cory/IO/Bmp.hpp>

#include <algorithm>
#include <fstream>
#include <limits>

namespace Cory::IO {
namespace {

constexpr size_t fileHeaderSize = 14;
constexpr size_t dibHeaderMinSize = 40;
constexpr size_t minBmpSize = fileHeaderSize + dibHeaderMinSize;

struct ParsedBmpInfo {
    uint32_t width{};
    uint32_t height{};
    uint32_t pixelDataOffset{};
    uint32_t colorTableOffset{};
    uint32_t colorTableEntryCount{};
    uint64_t rowStride{};
    bool topDown{};
    size_t r8ByteSize{};
};

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

[[nodiscard]] Result<ParsedBmpInfo> parseBmpInfo(std::span<const std::byte> bytes,
                                                 size_t backingSizeBytes)
{
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
    const auto colorsUsed = readU32(bytes, 46);

    if (widthSigned <= 0) return fail("BMP decode failed: invalid width");
    if (heightSigned == 0) return fail("BMP decode failed: invalid height");
    if (heightSigned == std::numeric_limits<int32_t>::min()) {
        return fail("BMP decode failed: invalid height");
    }
    if (planes != 1) return fail("BMP decode failed: invalid planes");
    if (compression != 0) return fail("BMP decode failed: compressed BMP is unsupported");
    if (bitCount != 8) {
        return fail("BMP decode failed: only 8-bit grayscale BMP is supported");
    }

    const auto width = static_cast<uint32_t>(widthSigned);
    const auto topDown = heightSigned < 0;
    const auto height = static_cast<uint32_t>(topDown ? -heightSigned : heightSigned);
    constexpr auto bytesPerPixel = uint64_t{1};
    const auto rowSizeRaw = static_cast<uint64_t>(width) * bytesPerPixel;
    const auto rowStride = (rowSizeRaw + 3ULL) & ~3ULL;
    const auto pixelArraySize = rowStride * static_cast<uint64_t>(height);
    const auto colorTableOffset = static_cast<uint32_t>(requiredHeaderBytes);

    if (pixelDataOffset > backingSizeBytes) {
        return fail("BMP decode failed: invalid pixel data offset");
    }
    if (pixelDataOffset < colorTableOffset) {
        return fail("BMP decode failed: invalid color table/pixel data layout");
    }

    const auto colorTableSizeBytes = static_cast<size_t>(pixelDataOffset - colorTableOffset);
    if ((colorTableSizeBytes % 4u) != 0u) {
        return fail("BMP decode failed: malformed color table");
    }
    const auto colorTableEntriesFromOffset = static_cast<uint32_t>(colorTableSizeBytes / 4u);
    const auto declaredColorEntries = colorsUsed == 0u ? 256u : colorsUsed;
    const auto colorTableEntryCount =
        colorTableEntriesFromOffset == 0u
            ? 0u
            : std::min(colorTableEntriesFromOffset, declaredColorEntries);

    if (pixelArraySize > std::numeric_limits<size_t>::max() ||
        static_cast<size_t>(pixelArraySize) > backingSizeBytes - pixelDataOffset) {
        return fail("BMP decode failed: truncated pixel data");
    }

    const auto pixelCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    if (pixelCount > std::numeric_limits<size_t>::max()) {
        return fail("BMP decode failed: image is too large");
    }

    ParsedBmpInfo info{};
    info.width = width;
    info.height = height;
    info.pixelDataOffset = pixelDataOffset;
    info.colorTableOffset = colorTableOffset;
    info.colorTableEntryCount = colorTableEntryCount;
    info.rowStride = rowStride;
    info.topDown = topDown;
    info.r8ByteSize = static_cast<size_t>(pixelCount);
    return info;
}

[[nodiscard]] auto toPublicInfo(const ParsedBmpInfo &info) -> BmpInfo
{
    return BmpInfo{
        .width = info.width,
        .height = info.height,
        .r8ByteSize = info.r8ByteSize,
    };
}

} // namespace

Result<BmpInfo> queryBmpInfo(std::span<const std::byte> bytes)
{
    auto parsed = parseBmpInfo(bytes, bytes.size());
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    return toPublicInfo(*parsed);
}

Result<BmpInfo> decodeBmp(std::span<const std::byte> bytes, std::span<std::byte> outputR8)
{
    auto parsed = parseBmpInfo(bytes, bytes.size());
    if (!parsed) return std::unexpected(std::move(parsed.error()));

    if (outputR8.size() != parsed->r8ByteSize) {
        return std::unexpected("BMP decode failed: output buffer size mismatch");
    }

    const auto *src = reinterpret_cast<const uint8_t *>(bytes.data());
    auto *dst = reinterpret_cast<uint8_t *>(outputR8.data());
    for (uint32_t y = 0; y < parsed->height; ++y) {
        const auto srcY = parsed->topDown ? y : (parsed->height - 1U - y);
        const auto srcRowOffset = static_cast<size_t>(parsed->pixelDataOffset) +
                                  static_cast<size_t>(srcY * parsed->rowStride);
        const auto dstRowOffset = static_cast<size_t>(y) * static_cast<size_t>(parsed->width);

        for (uint32_t x = 0; x < parsed->width; ++x) {
            const auto srcIndexOffset = srcRowOffset + static_cast<size_t>(x);
            const auto dstOffset = dstRowOffset + static_cast<size_t>(x);
            const auto paletteIndex = src[srcIndexOffset];
            if (parsed->colorTableEntryCount == 0u) {
                dst[dstOffset] = paletteIndex;
                continue;
            }

            if (paletteIndex >= parsed->colorTableEntryCount) {
                return std::unexpected("BMP decode failed: palette index out of bounds");
            }

            const auto paletteOffset = static_cast<size_t>(parsed->colorTableOffset) +
                                       static_cast<size_t>(paletteIndex) * 4u;
            // Color table entries are BGRA. Grayscale inputs have B==G==R; use R channel.
            dst[dstOffset] = src[paletteOffset + 2u];
        }
    }

    return toPublicInfo(*parsed);
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

    auto info = queryBmpInfo(bytes);
    if (!info) return std::unexpected(std::move(info.error()));

    BmpImage image{};
    image.width = info->width;
    image.height = info->height;
    image.pixelsR8.resize(info->r8ByteSize);

    auto decoded = decodeBmp(bytes, image.pixelsR8);
    if (!decoded) return std::unexpected(std::move(decoded.error()));

    return image;
}

} // namespace Cory::IO
