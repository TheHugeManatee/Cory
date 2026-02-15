#include <catch2/catch_test_macros.hpp>

#include <Cory/IO/Bmp.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
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

std::vector<std::byte> makeBmp(int32_t width,
                               int32_t height,
                               uint16_t bitsPerPixel,
                               std::span<const uint8_t> pixelBytes,
                               uint32_t compression = 0)
{
    const auto imageSize = static_cast<uint32_t>(pixelBytes.size());
    const auto pixelOffset = 14U + 40U;
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
    appendLe16(bytes, bitsPerPixel);
    appendLe32(bytes, compression);
    appendLe32(bytes, imageSize);
    appendLe32(bytes, 0);
    appendLe32(bytes, 0);
    appendLe32(bytes, 0);
    appendLe32(bytes, 0);

    for (auto value : pixelBytes) {
        appendU8(bytes, value);
    }

    return bytes;
}

std::array<uint8_t, 4> rgbaAt(const std::vector<std::byte> &rgba, size_t pixelIndex)
{
    const auto offset = pixelIndex * 4U;
    return {
        static_cast<uint8_t>(rgba[offset + 0]),
        static_cast<uint8_t>(rgba[offset + 1]),
        static_cast<uint8_t>(rgba[offset + 2]),
        static_cast<uint8_t>(rgba[offset + 3]),
    };
}

} // namespace

TEST_CASE("BMP decoder loads 24-bit bottom-up images", "[Cory/IO]")
{
    // 2x2 image. BMP rows are BGR with 4-byte row alignment.
    // Stored bottom-up: bottom row first, top row second.
    const std::vector<uint8_t> pixelBytes{
        // Bottom row: blue, white + row padding
        255, 0, 0, 255, 255, 255, 0, 0,
        // Top row: red, green + row padding
        0, 0, 255, 0, 255, 0, 0, 0,
    };
    const auto bmpBytes = makeBmp(2, 2, 24, pixelBytes);

    auto decoded = Cory::IO::decodeBmp(bmpBytes);
    REQUIRE(decoded);
    CHECK(decoded->width == 2);
    CHECK(decoded->height == 2);
    REQUIRE(decoded->pixelsRgba8.size() == 16);

    CHECK(rgbaAt(decoded->pixelsRgba8, 0) == std::array<uint8_t, 4>{255, 0, 0, 255});
    CHECK(rgbaAt(decoded->pixelsRgba8, 1) == std::array<uint8_t, 4>{0, 255, 0, 255});
    CHECK(rgbaAt(decoded->pixelsRgba8, 2) == std::array<uint8_t, 4>{0, 0, 255, 255});
    CHECK(rgbaAt(decoded->pixelsRgba8, 3) == std::array<uint8_t, 4>{255, 255, 255, 255});
}

TEST_CASE("BMP decoder loads 32-bit top-down images", "[Cory/IO]")
{
    // Height < 0 means top-down storage.
    const std::vector<uint8_t> pixelBytes{
        // Top row pixel: RGBA(10,20,30,40) encoded as BGRA
        30, 20, 10, 40,
        // Bottom row pixel: RGBA(50,60,70,80) encoded as BGRA
        70, 60, 50, 80,
    };
    const auto bmpBytes = makeBmp(1, -2, 32, pixelBytes);

    auto decoded = Cory::IO::decodeBmp(bmpBytes);
    REQUIRE(decoded);
    CHECK(decoded->width == 1);
    CHECK(decoded->height == 2);
    REQUIRE(decoded->pixelsRgba8.size() == 8);

    CHECK(rgbaAt(decoded->pixelsRgba8, 0) == std::array<uint8_t, 4>{10, 20, 30, 40});
    CHECK(rgbaAt(decoded->pixelsRgba8, 1) == std::array<uint8_t, 4>{50, 60, 70, 80});
}

TEST_CASE("BMP decoder rejects unsupported format", "[Cory/IO]")
{
    const std::vector<uint8_t> pixelBytes{0, 1, 2, 3};
    const auto bmpBytes = makeBmp(1, 1, 8, pixelBytes);

    const auto decoded = Cory::IO::decodeBmp(bmpBytes);
    REQUIRE_FALSE(decoded);
    CHECK(decoded.error().find("24-bit and 32-bit") != std::string::npos);
}

TEST_CASE("BMP decoder rejects truncated pixel payload", "[Cory/IO]")
{
    const std::vector<uint8_t> pixelBytes{
        // 1x1 24-bit requires 4 bytes because of row padding; supply only 3.
        0, 0, 255,
    };
    const auto bmpBytes = makeBmp(1, 1, 24, pixelBytes);

    const auto decoded = Cory::IO::decodeBmp(bmpBytes);
    REQUIRE_FALSE(decoded);
    CHECK(decoded.error().find("truncated pixel data") != std::string::npos);
}

TEST_CASE("BMP loader reads files from disk", "[Cory/IO]")
{
    const std::vector<uint8_t> pixelBytes{
        // 1x1 24-bit red pixel + row padding
        0, 0, 255, 0,
    };
    const auto bmpBytes = makeBmp(1, 1, 24, pixelBytes);

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
    CHECK(loaded->width == 1);
    CHECK(loaded->height == 1);
    CHECK(rgbaAt(loaded->pixelsRgba8, 0) == std::array<uint8_t, 4>{255, 0, 0, 255});
}

TEST_CASE("BMP loader reads checked-in python-generated grayscale BMP", "[Cory/IO]")
{
    const auto testDataPath = std::filesystem::path{__FILE__}.parent_path() / "data" /
                              "gray16x16_uncompressed_24bpp.bmp";
    const auto loaded = Cory::IO::loadBmp(testDataPath);

    REQUIRE(loaded);
    CHECK(loaded->width == 16);
    CHECK(loaded->height == 16);
    REQUIRE(loaded->pixelsRgba8.size() == 16u * 16u * 4u);

    // top-left pixel
    CHECK(rgbaAt(loaded->pixelsRgba8, 0) == std::array<uint8_t, 4>{0, 0, 0, 255});
    // center-ish pixel (x=8,y=8 => 136)
    CHECK(rgbaAt(loaded->pixelsRgba8, 8u + 8u * 16u) ==
          std::array<uint8_t, 4>{136, 136, 136, 255});
    // bottom-right pixel (255)
    CHECK(rgbaAt(loaded->pixelsRgba8, 15u + 15u * 16u) ==
          std::array<uint8_t, 4>{255, 255, 255, 255});
}
