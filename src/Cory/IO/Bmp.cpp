#include <Cory/IO/Bmp.hpp>

#include <gsl/narrow>

#include <algorithm>
#include <fstream>
#include <limits>

namespace Cory::IO {
namespace {

constexpr size_t fileHeaderSize = 14;
constexpr size_t dibHeaderMinSize = 40;
constexpr size_t minBmpSize = fileHeaderSize + dibHeaderMinSize;
constexpr uint16_t bmpPlanes = 1;
constexpr uint16_t bmpBitCountRgba8 = 32;
constexpr uint32_t bmpCompressionRgb = 0;
constexpr size_t rgba8BytesPerPixel = 4;

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

[[nodiscard]] Result<std::vector<std::byte>> loadFileBytes(const std::filesystem::path &path)
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
    return bytes;
}

void appendU16(std::vector<std::byte> &out, uint16_t value)
{
    out.push_back(static_cast<std::byte>(value & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void appendU32(std::vector<std::byte> &out, uint32_t value)
{
    out.push_back(static_cast<std::byte>(value & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
}

void appendI32(std::vector<std::byte> &out, int32_t value)
{
    appendU32(out, static_cast<uint32_t>(value));
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

    // Conversions up front - assuming everything fits into uint32_t
    const auto parsedData = *parsed; // Avoid repeated struct member access in the inner loop
    const auto rowStride = gsl::narrow<uint32_t>(parsedData.rowStride);
    const auto pixelDataOffset = gsl::narrow<uint32_t>(parsedData.pixelDataOffset);
    std::array<uint8_t, 256> grayscalePalette{};
    bool palletteIsIdentity = true;

    if (parsedData.colorTableEntryCount == 0u) {
        // No color table means the palette is the identity mapping (grayscale)
        for (uint32_t i = 0; i < 256u; ++i) {
            grayscalePalette[i] = static_cast<uint8_t>(i);
        }
    }
    else {
        // Pre-extract the grayscale values from the color table for a faster lookup in the inner
        // loop
        for (uint32_t i = 0; i < parsedData.colorTableEntryCount; ++i) {
            const auto paletteOffset =
                static_cast<size_t>(parsedData.colorTableOffset) + static_cast<size_t>(i) * 4u;

            // Color table entries are BGRA. Grayscale inputs have B==G==R; use R channel.
            grayscalePalette[i] = src[paletteOffset + 2u];
            if (grayscalePalette[i] != i) {
                palletteIsIdentity = false;
            }
        }
    }

    // If the palette is the identity mapping, we can skip the lookup and just copy the bytes
    if (palletteIsIdentity) {
        if (parsedData.topDown == true) {
            // If there is no row padding we can bulk-copy the whole image. Otherwise, copy row
            // payload bytes only (exclude BMP row padding from destination).
            if (rowStride == parsedData.width) {
                std::copy_n(src + pixelDataOffset, parsedData.r8ByteSize, dst);
                return toPublicInfo(*parsed);
            }

            for (uint32_t y = 0; y < parsedData.height; ++y) {
                const auto srcRowOffset = pixelDataOffset + y * rowStride;
                const auto dstRowOffset = y * parsedData.width;
                std::copy_n(src + srcRowOffset, parsedData.width, dst + dstRowOffset);
            }
            return toPublicInfo(*parsed);
        }

        for (uint32_t y = 0; y < parsedData.height; ++y) {
            const auto srcY = parsedData.topDown ? y : (parsedData.height - 1u - y);
            const auto srcRowOffset = pixelDataOffset + srcY * rowStride;
            const auto dstRowOffset = y * parsedData.width;

            std::copy_n(src + srcRowOffset, parsedData.width, dst + dstRowOffset);
        }
        return toPublicInfo(*parsed);
    }

    // Slow path: palette is not the identity mapping, so we have to do a lookup for each pixel
    for (uint32_t y = 0; y < parsedData.height; ++y) {
        const auto srcY = parsedData.topDown ? y : (parsedData.height - 1u - y);
        const auto srcRowOffset = pixelDataOffset + srcY * rowStride;
        const auto dstRowOffset = y * parsedData.width;

        for (uint32_t x = 0; x < parsedData.width; ++x) {
            const auto srcIndexOffset = srcRowOffset + x;
            const auto dstOffset = dstRowOffset + x;
            const auto paletteIndex = src[srcIndexOffset];

            dst[dstOffset] = grayscalePalette[paletteIndex];
        }
    }

    return toPublicInfo(*parsed);
}

Result<BmpImage> loadBmp(const std::filesystem::path &path)
{
    auto bytes = loadFileBytes(path);
    if (!bytes) return std::unexpected(std::move(bytes.error()));

    auto info = queryBmpInfo(*bytes);
    if (!info) return std::unexpected(std::move(info.error()));

    BmpImage image{};
    image.width = info->width;
    image.height = info->height;
    image.pixelsR8.resize(info->r8ByteSize);

    auto decoded = decodeBmp(*bytes, image.pixelsR8);
    if (!decoded) return std::unexpected(std::move(decoded.error()));

    return image;
}

Result<BmpImageRgba8> decodeBmpRgba8(std::span<const std::byte> bytes)
{
    if (bytes.size() < fileHeaderSize + dibHeaderMinSize || bytes[0] != std::byte{'B'} ||
        bytes[1] != std::byte{'M'}) {
        return std::unexpected("BMP decode failed: invalid signature");
    }

    const auto pixelOffset = readU32(bytes, 10);
    const auto headerSize = readU32(bytes, 14);
    const auto widthSigned = readI32(bytes, 18);
    const auto heightSigned = readI32(bytes, 22);
    const auto planes = readU16(bytes, 26);
    const auto bitCount = readU16(bytes, 28);
    const auto compression = readU32(bytes, 30);
    if (headerSize != dibHeaderMinSize || widthSigned <= 0 || heightSigned == 0 ||
        heightSigned == std::numeric_limits<int32_t>::min() || planes != bmpPlanes ||
        bitCount != bmpBitCountRgba8 || compression != bmpCompressionRgb) {
        return std::unexpected("BMP decode failed: expected uncompressed 32-bit BGRA BMP");
    }

    const auto absHeight = heightSigned < 0 ? -heightSigned : heightSigned;
    const auto width = static_cast<uint32_t>(widthSigned);
    const auto height = static_cast<uint32_t>(absHeight);
    const auto requiredBytes64 =
        static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * rgba8BytesPerPixel;
    if (requiredBytes64 > std::numeric_limits<size_t>::max()) {
        return std::unexpected("BMP decode failed: image is too large");
    }
    const auto requiredBytes = static_cast<size_t>(requiredBytes64);
    if (pixelOffset > bytes.size() || requiredBytes > bytes.size() - pixelOffset) {
        return std::unexpected("BMP decode failed: truncated pixel data");
    }

    BmpImageRgba8 image{.width = width, .height = height};
    image.pixelsRgba8.resize(requiredBytes);
    const bool topDown = heightSigned < 0;
    for (uint32_t y = 0; y < height; ++y) {
        const auto srcY = topDown ? y : (height - 1U - y);
        const auto *src =
            bytes.data() + pixelOffset +
            static_cast<size_t>(srcY) * static_cast<size_t>(width) * rgba8BytesPerPixel;
        auto *dst = image.pixelsRgba8.data() +
                    static_cast<size_t>(y) * static_cast<size_t>(width) * rgba8BytesPerPixel;
        for (uint32_t x = 0; x < width; ++x) {
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }
    return image;
}

Result<BmpImageRgba8> loadBmpRgba8(const std::filesystem::path &path)
{
    auto bytes = loadFileBytes(path);
    if (!bytes) return std::unexpected(std::move(bytes.error()));
    return decodeBmpRgba8(*bytes);
}

Result<void> writeBmpRgba8(const std::filesystem::path &path, const BmpImageRgba8 &image)
{
    if (image.width == 0 || image.height == 0 ||
        image.width > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
        image.height > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        return std::unexpected("BMP write failed: invalid image dimensions");
    }

    const auto expectedBytes64 = static_cast<uint64_t>(image.width) *
                                 static_cast<uint64_t>(image.height) * rgba8BytesPerPixel;
    constexpr auto headerBytes = fileHeaderSize + dibHeaderMinSize;
    if (expectedBytes64 > std::numeric_limits<uint32_t>::max() - headerBytes ||
        expectedBytes64 > std::numeric_limits<size_t>::max()) {
        return std::unexpected("BMP write failed: image is too large");
    }
    const auto expectedBytes = static_cast<size_t>(expectedBytes64);
    if (image.pixelsRgba8.size() != expectedBytes) {
        return std::unexpected("BMP write failed: pixel buffer size mismatch");
    }
    const auto pixelBytes = static_cast<uint32_t>(expectedBytes);
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    const auto fileSize = static_cast<uint32_t>(headerBytes) + pixelBytes;
    std::vector<std::byte> bytes;
    bytes.reserve(fileSize);
    bytes.push_back(static_cast<std::byte>('B'));
    bytes.push_back(static_cast<std::byte>('M'));
    appendU32(bytes, fileSize);
    appendU16(bytes, 0);
    appendU16(bytes, 0);
    appendU32(bytes, static_cast<uint32_t>(fileHeaderSize + dibHeaderMinSize));
    appendU32(bytes, static_cast<uint32_t>(dibHeaderMinSize));
    appendI32(bytes, gsl::narrow<int32_t>(image.width));
    appendI32(bytes, -gsl::narrow<int32_t>(image.height));
    appendU16(bytes, bmpPlanes);
    appendU16(bytes, bmpBitCountRgba8);
    appendU32(bytes, bmpCompressionRgb);
    appendU32(bytes, pixelBytes);
    appendI32(bytes, 0);
    appendI32(bytes, 0);
    appendU32(bytes, 0);
    appendU32(bytes, 0);

    for (size_t i = 0; i < image.pixelsRgba8.size(); i += 4) {
        bytes.push_back(image.pixelsRgba8[i + 2]);
        bytes.push_back(image.pixelsRgba8[i + 1]);
        bytes.push_back(image.pixelsRgba8[i + 0]);
        bytes.push_back(image.pixelsRgba8[i + 3]);
    }

    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    if (!file.is_open()) {
        return std::unexpected("BMP write failed: could not open file '" + path.string() + "'");
    }
    file.write(reinterpret_cast<const char *>(bytes.data()),
               gsl::narrow<std::streamsize>(bytes.size()));
    if (!file) {
        return std::unexpected("BMP write failed: failed to write file '" + path.string() + "'");
    }
    return {};
}

} // namespace Cory::IO
