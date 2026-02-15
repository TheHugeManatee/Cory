#include <catch2/catch_test_macros.hpp>

#include <DatasetLoader.hpp>

#include <cppcoro/sync_wait.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

std::vector<std::byte> makeSolidGrayBmp24(uint32_t width, uint32_t height, uint8_t grayValue)
{
    const auto rowStride = ((width * 3u) + 3u) & ~3u;
    const auto imageSize = rowStride * height;
    const auto pixelOffset = 14u + 40u;
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
    appendLe32(bytes, width);
    appendLe32(bytes, height);
    appendLe16(bytes, 1);
    appendLe16(bytes, 24);
    appendLe32(bytes, 0);
    appendLe32(bytes, imageSize);
    appendLe32(bytes, 0);
    appendLe32(bytes, 0);
    appendLe32(bytes, 0);
    appendLe32(bytes, 0);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            appendU8(bytes, grayValue);
            appendU8(bytes, grayValue);
            appendU8(bytes, grayValue);
        }
        for (uint32_t pad = width * 3u; pad < rowStride; ++pad) {
            appendU8(bytes, 0);
        }
    }

    return bytes;
}

void writeBytes(const std::filesystem::path &path, const std::vector<std::byte> &bytes)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    REQUIRE(file.is_open());
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(file.good());
}

struct TempDir {
    std::filesystem::path path{};

    TempDir()
    {
        const auto nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               ("cory_dataset_loader_test_" + std::to_string(static_cast<long long>(nowNs)));
        std::filesystem::create_directories(path);
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

std::byte voxelAt(const Cory::LoadedVolume &volume, uint32_t x, uint32_t y, uint32_t z)
{
    const auto index = static_cast<size_t>(z) * static_cast<size_t>(volume.dimensions.x) *
                           static_cast<size_t>(volume.dimensions.y) +
                       static_cast<size_t>(y) * static_cast<size_t>(volume.dimensions.x) +
                       static_cast<size_t>(x);
    return volume.voxelsR8[index];
}

} // namespace

TEST_CASE("DatasetLoader loads contiguous BMP stack into packed r8 volume", "[DatasetLoader]")
{
    TempDir temp;
    writeBytes(temp.path / "scan_12.bmp", makeSolidGrayBmp24(2, 2, 12));
    writeBytes(temp.path / "scan_10.bmp", makeSolidGrayBmp24(2, 2, 10));
    writeBytes(temp.path / "scan_11.bmp", makeSolidGrayBmp24(2, 2, 11));

    Cory::DatasetLoader loader{4};
    auto result = cppcoro::sync_wait(loader.loadBmpStack(Cory::LoadStackRequest{
        .directory = temp.path,
        .pattern = "*.bmp",
        .maxConcurrency = 2,
    }));

    REQUIRE(result);
    CHECK(result->dimensions == glm::uvec3{2u, 2u, 3u});
    REQUIRE(result->voxelsR8.size() == 12u);
    CHECK(static_cast<uint8_t>(voxelAt(*result, 0u, 0u, 0u)) == 10u);
    CHECK(static_cast<uint8_t>(voxelAt(*result, 1u, 1u, 1u)) == 11u);
    CHECK(static_cast<uint8_t>(voxelAt(*result, 0u, 1u, 2u)) == 12u);

    REQUIRE(result->orderedSlicePaths.size() == 3u);
    CHECK(result->orderedSlicePaths[0].filename().string() == "scan_10.bmp");
    CHECK(result->orderedSlicePaths[1].filename().string() == "scan_11.bmp");
    CHECK(result->orderedSlicePaths[2].filename().string() == "scan_12.bmp");
}

TEST_CASE("DatasetLoader rejects empty match set", "[DatasetLoader]")
{
    TempDir temp;

    Cory::DatasetLoader loader{2};
    auto result = cppcoro::sync_wait(loader.loadBmpStack(Cory::LoadStackRequest{
        .directory = temp.path,
        .pattern = "*.bmp",
    }));

    REQUIRE_FALSE(result);
    CHECK(result.error().find("No files matching") != std::string::npos);
}

TEST_CASE("DatasetLoader rejects non-contiguous numeric suffixes", "[DatasetLoader]")
{
    TempDir temp;
    writeBytes(temp.path / "slice_0.bmp", makeSolidGrayBmp24(2, 2, 10));
    writeBytes(temp.path / "slice_1.bmp", makeSolidGrayBmp24(2, 2, 20));
    writeBytes(temp.path / "slice_3.bmp", makeSolidGrayBmp24(2, 2, 30));

    Cory::DatasetLoader loader{3};
    auto result = cppcoro::sync_wait(loader.loadBmpStack(Cory::LoadStackRequest{
        .directory = temp.path,
        .pattern = "*.bmp",
    }));

    REQUIRE_FALSE(result);
    CHECK(result.error().find("Slice index gap") != std::string::npos);
}

TEST_CASE("DatasetLoader rejects stack with inconsistent dimensions", "[DatasetLoader]")
{
    TempDir temp;
    writeBytes(temp.path / "slice_0.bmp", makeSolidGrayBmp24(2, 2, 10));
    writeBytes(temp.path / "slice_1.bmp", makeSolidGrayBmp24(3, 2, 20));

    Cory::DatasetLoader loader{2};
    auto result = cppcoro::sync_wait(loader.loadBmpStack(Cory::LoadStackRequest{
        .directory = temp.path,
        .pattern = "*.bmp",
        .maxConcurrency = 2,
    }));

    REQUIRE_FALSE(result);
    CHECK(result.error().find("dimensions mismatch") != std::string::npos);
}

TEST_CASE("DatasetLoader rejects corrupt BMP in matched set", "[DatasetLoader]")
{
    TempDir temp;
    writeBytes(temp.path / "slice_0.bmp", makeSolidGrayBmp24(2, 2, 10));
    writeBytes(temp.path / "slice_1.bmp", std::vector<std::byte>{std::byte{0x00}, std::byte{0x01}});

    Cory::DatasetLoader loader{2};
    auto result = cppcoro::sync_wait(loader.loadBmpStack(Cory::LoadStackRequest{
        .directory = temp.path,
        .pattern = "*.bmp",
    }));

    REQUIRE_FALSE(result);
    CHECK(result.error().find("failed") != std::string::npos);
}

TEST_CASE("DatasetLoader ordering is deterministic from numeric suffix", "[DatasetLoader]")
{
    TempDir temp;
    writeBytes(temp.path / "foo_101.bmp", makeSolidGrayBmp24(1, 1, 41));
    writeBytes(temp.path / "foo_100.bmp", makeSolidGrayBmp24(1, 1, 40));
    writeBytes(temp.path / "foo_102.bmp", makeSolidGrayBmp24(1, 1, 42));

    Cory::DatasetLoader loader{1};
    auto result = cppcoro::sync_wait(loader.loadBmpStack(Cory::LoadStackRequest{
        .directory = temp.path,
        .pattern = "foo_*.bmp",
    }));

    REQUIRE(result);
    REQUIRE(result->orderedSlicePaths.size() == 3u);
    CHECK(result->orderedSlicePaths[0].filename().string() == "foo_100.bmp");
    CHECK(result->orderedSlicePaths[1].filename().string() == "foo_101.bmp");
    CHECK(result->orderedSlicePaths[2].filename().string() == "foo_102.bmp");
}

TEST_CASE("DatasetLoader fails large parallel load when one slice is corrupt", "[DatasetLoader]")
{
    TempDir temp;
    for (size_t i = 0; i < 32u; ++i) {
        const auto filename = temp.path / ("slice_" + std::to_string(i) + ".bmp");
        if (i == 7u) {
            writeBytes(filename, std::vector<std::byte>{std::byte{0x42}});
            continue;
        }

        writeBytes(filename, makeSolidGrayBmp24(8, 8, static_cast<uint8_t>(i)));
    }

    Cory::DatasetLoader loader{8};
    auto result = cppcoro::sync_wait(loader.loadBmpStack(Cory::LoadStackRequest{
        .directory = temp.path,
        .pattern = "slice_*.bmp",
        .maxConcurrency = 8,
    }));

    REQUIRE_FALSE(result);
    CHECK((result.error().find("Slice") != std::string::npos ||
           result.error().find("cancelled") != std::string::npos));
}
