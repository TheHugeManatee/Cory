#include <catch2/catch_test_macros.hpp>

#include <Cory/IO/Bmp.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace {

void appendU8(std::vector<std::byte> &out, uint8_t value)
{
    out.push_back(static_cast<std::byte>(value));
}

void appendLe16(std::vector<std::byte> &out, uint16_t value)
{
    appendU8(out, static_cast<uint8_t>(value & 0xFFU));
    appendU8(out, static_cast<uint8_t>((value >> 8U) & 0xFFU));
}

void appendLe32(std::vector<std::byte> &out, uint32_t value)
{
    appendU8(out, static_cast<uint8_t>(value & 0xFFU));
    appendU8(out, static_cast<uint8_t>((value >> 8U) & 0xFFU));
    appendU8(out, static_cast<uint8_t>((value >> 16U) & 0xFFU));
    appendU8(out, static_cast<uint8_t>((value >> 24U) & 0xFFU));
}

std::vector<std::byte> makeGrayBmp8(int32_t width,
                                    int32_t height,
                                    std::span<const uint8_t> pixelIndices,
                                    bool withPalette = true)
{
    const auto absHeight = static_cast<uint32_t>(height < 0 ? -height : height);
    const auto widthU = static_cast<uint32_t>(width);
    const auto rowStride = (widthU + 3u) & ~3u;
    const auto imageSize = rowStride * absHeight;
    const auto paletteSize = withPalette ? 256u * 4u : 0u;
    const auto pixelOffset = 14u + 40u + paletteSize;
    const auto fileSize = pixelOffset + imageSize;

    std::vector<std::byte> bytes;
    bytes.reserve(fileSize);

    appendU8(bytes, 'B');
    appendU8(bytes, 'M');
    appendLe32(bytes, fileSize);
    appendLe16(bytes, 0);
    appendLe16(bytes, 0);
    appendLe32(bytes, pixelOffset);

    appendLe32(bytes, 40);
    appendLe32(bytes, static_cast<uint32_t>(width));
    appendLe32(bytes, static_cast<uint32_t>(height));
    appendLe16(bytes, 1);
    appendLe16(bytes, 8);
    appendLe32(bytes, 0);
    appendLe32(bytes, imageSize);
    appendLe32(bytes, 0);
    appendLe32(bytes, 0);
    appendLe32(bytes, withPalette ? 256u : 0u);
    appendLe32(bytes, 0);

    if (withPalette) {
        for (uint32_t i = 0; i < 256u; ++i) {
            appendU8(bytes, static_cast<uint8_t>(i));
            appendU8(bytes, static_cast<uint8_t>(i));
            appendU8(bytes, static_cast<uint8_t>(i));
            appendU8(bytes, 0);
        }
    }

    const auto expectedPixels = static_cast<size_t>(widthU) * static_cast<size_t>(absHeight);
    REQUIRE(pixelIndices.size() == expectedPixels);

    for (uint32_t y = 0; y < absHeight; ++y) {
        const auto srcY = height < 0 ? y : (absHeight - 1u - y);
        const auto rowStart = static_cast<size_t>(srcY) * static_cast<size_t>(widthU);
        for (uint32_t x = 0; x < widthU; ++x) {
            appendU8(bytes, pixelIndices[rowStart + x]);
        }
        for (uint32_t pad = widthU; pad < rowStride; ++pad) {
            appendU8(bytes, 0);
        }
    }

    return bytes;
}

uint8_t grayAt(std::span<const std::byte> pixels, size_t pixelIndex)
{
    return static_cast<uint8_t>(pixels[pixelIndex]);
}

} // namespace

TEST_CASE("BMP decoder loads 8-bit grayscale bottom-up images", "[Cory/IO]")
{
    const std::vector<uint8_t> pixelIndices{
        10,
        20,
        30,
        40,
    };
    const auto bmpBytes = makeGrayBmp8(2, 2, pixelIndices, true);

    auto info = Cory::IO::queryBmpInfo(bmpBytes);
    REQUIRE(info);
    CHECK(info->width == 2u);
    CHECK(info->height == 2u);
    CHECK(info->r8ByteSize == 4u);

    std::vector<std::byte> outR8(info->r8ByteSize);
    auto decoded = Cory::IO::decodeBmp(bmpBytes, outR8);
    REQUIRE(decoded);

    CHECK(grayAt(outR8, 0u) == 10u);
    CHECK(grayAt(outR8, 1u) == 20u);
    CHECK(grayAt(outR8, 2u) == 30u);
    CHECK(grayAt(outR8, 3u) == 40u);
}

TEST_CASE("BMP decoder loads 8-bit grayscale top-down images", "[Cory/IO]")
{
    const std::vector<uint8_t> pixelIndices{
        50,
        80,
    };
    const auto bmpBytes = makeGrayBmp8(1, -2, pixelIndices, true);

    std::vector<std::byte> outR8(2);
    auto decoded = Cory::IO::decodeBmp(bmpBytes, outR8);
    REQUIRE(decoded);

    CHECK(grayAt(outR8, 0u) == 50u);
    CHECK(grayAt(outR8, 1u) == 80u);
}

TEST_CASE("BMP decoder rejects unsupported format", "[Cory/IO]")
{
    const std::vector<uint8_t> pixelBytes{0, 0, 255, 0};
    // 24-bit image payload for 1x1 + padding
    std::vector<std::byte> bytes;
    bytes.reserve(14 + 40 + pixelBytes.size());
    appendU8(bytes, 'B');
    appendU8(bytes, 'M');
    appendLe32(bytes, 14u + 40u + 4u);
    appendLe16(bytes, 0);
    appendLe16(bytes, 0);
    appendLe32(bytes, 14u + 40u);
    appendLe32(bytes, 40);
    appendLe32(bytes, 1u);
    appendLe32(bytes, 1u);
    appendLe16(bytes, 1u);
    appendLe16(bytes, 24u);
    appendLe32(bytes, 0u);
    appendLe32(bytes, 4u);
    appendLe32(bytes, 0u);
    appendLe32(bytes, 0u);
    appendLe32(bytes, 0u);
    appendLe32(bytes, 0u);
    for (auto v : pixelBytes)
        appendU8(bytes, v);

    std::vector<std::byte> outR8(1);
    const auto decoded = Cory::IO::decodeBmp(bytes, outR8);
    REQUIRE_FALSE(decoded);
    CHECK(decoded.error().find("8-bit grayscale") != std::string::npos);
}

TEST_CASE("BMP loader reads files from disk", "[Cory/IO]")
{
    const std::vector<uint8_t> pixelIndices{77};
    const auto bmpBytes = makeGrayBmp8(1, 1, pixelIndices, true);

    const auto path = std::filesystem::temp_directory_path() / "cory_bmp_loader_test.bmp";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        REQUIRE(out.is_open());
        out.write(reinterpret_cast<const char *>(bmpBytes.data()),
                  static_cast<std::streamsize>(bmpBytes.size()));
    }

    const auto loaded = Cory::IO::loadBmp(path);
    std::filesystem::remove(path);

    REQUIRE(loaded);
    CHECK(loaded->width == 1u);
    CHECK(loaded->height == 1u);
    REQUIRE(loaded->pixelsR8.size() == 1u);
    CHECK(grayAt(loaded->pixelsR8, 0u) == 77u);
}

TEST_CASE("BMP decoder rejects output buffer size mismatch", "[Cory/IO]")
{
    const std::vector<uint8_t> pixelIndices{12};
    const auto bmpBytes = makeGrayBmp8(1, 1, pixelIndices, true);

    std::vector<std::byte> outPixels(2);
    const auto loaded = Cory::IO::decodeBmp(bmpBytes, outPixels);

    REQUIRE_FALSE(loaded);
    CHECK(loaded.error().find("output buffer size mismatch") != std::string::npos);
}
