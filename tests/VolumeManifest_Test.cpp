#include <catch2/catch_test_macros.hpp>

#include <VolumeManifest.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

std::string makeManifestText(std::string_view previewDimensions, std::string_view fullDimensions)
{
    return "{\n"
           "  \"cory_volume_manifest_version\": 1,\n"
           "  \"dataset_id\": \"test_dataset\",\n"
           "  \"voxel_format\": \"r8_unorm\",\n"
           "  \"endianness\": \"little\",\n"
           "  \"spacing_mm\": [1.0, 1.0, 1.0],\n"
           "  \"source_dimensions\": [16, 16, 16],\n"
           "  \"preview_blob\": \"preview.raw\",\n"
           "  \"preview_dimensions\": [" +
           std::string{previewDimensions} +
           "],\n"
           "  \"preview_byte_size\": 512,\n"
           "  \"full_blob\": \"full.raw\",\n"
           "  \"full_dimensions\": [" +
           std::string{fullDimensions} +
           "],\n"
           "  \"full_byte_size\": 4096,\n"
           "  \"normalization\": \"none\"\n"
           "}\n";
}

fs::path writeManifestFile(std::string_view contents, std::string_view name)
{
    const auto dir = fs::temp_directory_path() / "cory_volume_manifest_tests";
    fs::create_directories(dir);
    const auto path = dir / std::string{name};
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
    return path;
}

} // namespace

TEST_CASE("VolumeManifest rejects zero preview_dimensions", "[VolumeManifest]")
{
    const auto manifestPath =
        writeManifestFile(makeManifestText("0, 16, 16", "32, 32, 32"), "zero_preview.cvol");

    VolumeManifest manifest{};
    std::string error{};
    const bool ok = loadVolumeManifest(manifestPath, manifest, error);
    fs::remove(manifestPath);

    REQUIRE_FALSE(ok);
    CHECK(error.find("Invalid preview_dimensions") != std::string::npos);
}

TEST_CASE("VolumeManifest rejects zero full_dimensions", "[VolumeManifest]")
{
    const auto manifestPath =
        writeManifestFile(makeManifestText("16, 16, 16", "0, 32, 32"), "zero_full.cvol");

    VolumeManifest manifest{};
    std::string error{};
    const bool ok = loadVolumeManifest(manifestPath, manifest, error);
    fs::remove(manifestPath);

    REQUIRE_FALSE(ok);
    CHECK(error.find("Invalid full_dimensions") != std::string::npos);
}

TEST_CASE("VolumeManifest accepts non-zero preview and full dimensions", "[VolumeManifest]")
{
    const auto manifestPath =
        writeManifestFile(makeManifestText("16, 16, 16", "32, 32, 32"), "valid_dims.cvol");

    VolumeManifest manifest{};
    std::string error{};
    const bool ok = loadVolumeManifest(manifestPath, manifest, error);
    fs::remove(manifestPath);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(manifest.preview.dimensions == glm::uvec3{16u, 16u, 16u});
    CHECK(manifest.full.dimensions == glm::uvec3{32u, 32u, 32u});
}
