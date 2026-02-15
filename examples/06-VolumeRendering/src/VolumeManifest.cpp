#include "VolumeManifest.hpp"

#include <Cory/Base/Log.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <limits>

using Json = nlohmann::json;

namespace {

[[nodiscard]] bool isNonZeroDimensions(const glm::uvec3 &value)
{
    return value.x > 0u && value.y > 0u && value.z > 0u;
}

[[nodiscard]] bool parseUvec3(const Json &j, glm::uvec3 &valueOut)
{
    if (!j.is_array() || j.size() != 3) {
        return false;
    }
    std::array<uint32_t, 3> values{};
    for (size_t i = 0; i < values.size(); ++i) {
        if (!j[i].is_number_unsigned() && !j[i].is_number_integer()) {
            return false;
        }
        const auto value = j[i].get<int64_t>();
        if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        values[i] = static_cast<uint32_t>(value);
    }
    valueOut = glm::uvec3{values[0], values[1], values[2]};
    return true;
}

[[nodiscard]] bool parseVec3(const Json &j, glm::vec3 &valueOut)
{
    if (!j.is_array() || j.size() != 3) {
        return false;
    }
    std::array<float, 3> values{};
    for (size_t i = 0; i < values.size(); ++i) {
        if (!j[i].is_number()) {
            return false;
        }
        values[i] = j[i].get<float>();
    }
    valueOut = glm::vec3{values[0], values[1], values[2]};
    return true;
}

} // namespace

bool loadVolumeManifest(const std::filesystem::path &manifestPath,
                        VolumeManifest &manifestOut,
                        std::string &errorOut)
{
    std::ifstream in(manifestPath);
    if (!in.is_open()) {
        errorOut = fmt::format("Failed to open volume manifest '{}'", manifestPath.string());
        return false;
    }

    Json root;
    try {
        in >> root;
    } catch (const nlohmann::json::parse_error &e) {
        errorOut = fmt::format(
            "Invalid JSON manifest '{}': {}", manifestPath.string(), e.what());
        return false;
    }

    if (!root.is_object()) {
        errorOut = fmt::format("Invalid manifest '{}': root must be a JSON object",
                               manifestPath.string());
        return false;
    }

    auto require = [&](const char *key) -> const Json * {
        if (auto it = root.find(key); it != root.end()) {
            return &(*it);
        }
        errorOut = fmt::format("Missing required key '{}' in '{}'", key, manifestPath.string());
        return nullptr;
    };

    const auto *version = require("cory_volume_manifest_version");
    const auto *datasetId = require("dataset_id");
    const auto *voxelFormat = require("voxel_format");
    const auto *endianness = require("endianness");
    const auto *spacing = require("spacing_mm");
    const auto *sourceDimensions = require("source_dimensions");
    const auto *previewBlob = require("preview_blob");
    const auto *previewDimensions = require("preview_dimensions");
    const auto *previewByteSize = require("preview_byte_size");
    const auto *fullBlob = require("full_blob");
    const auto *fullDimensions = require("full_dimensions");
    const auto *fullByteSize = require("full_byte_size");
    const auto *normalization = require("normalization");

    if (version == nullptr || datasetId == nullptr || voxelFormat == nullptr ||
        endianness == nullptr || spacing == nullptr || sourceDimensions == nullptr ||
        previewBlob == nullptr || previewDimensions == nullptr || previewByteSize == nullptr ||
        fullBlob == nullptr || fullDimensions == nullptr || fullByteSize == nullptr ||
        normalization == nullptr) {
        return false;
    }

    if (!version->is_number_integer() || version->get<int>() != 1) {
        errorOut = fmt::format("Unsupported cory_volume_manifest_version in '{}'",
                               manifestPath.string());
        return false;
    }
    if (!voxelFormat->is_string() || voxelFormat->get<std::string>() != "r8_unorm") {
        errorOut = fmt::format(
            "Unsupported voxel_format in '{}'", manifestPath.string());
        return false;
    }
    if (!endianness->is_string() || endianness->get<std::string>() != "little") {
        errorOut = fmt::format("Unsupported endianness in '{}'", manifestPath.string());
        return false;
    }

    VolumeManifest manifest{};
    manifest.manifestPath = manifestPath;
    if (!datasetId->is_string()) {
        errorOut = fmt::format("Invalid dataset_id in '{}'", manifestPath.string());
        return false;
    }
    manifest.datasetId = datasetId->get<std::string>();
    if (!normalization->is_string()) {
        errorOut = fmt::format("Invalid normalization in '{}'", manifestPath.string());
        return false;
    }
    manifest.normalization = normalization->get<std::string>();

    if (!parseVec3(*spacing, manifest.spacingMm)) {
        errorOut = fmt::format("Invalid spacing_mm in '{}'", manifestPath.string());
        return false;
    }
    if (!parseUvec3(*sourceDimensions, manifest.sourceDimensions)) {
        errorOut = fmt::format("Invalid source_dimensions in '{}'", manifestPath.string());
        return false;
    }

    if (!previewBlob->is_string()) {
        errorOut = fmt::format("Invalid preview_blob in '{}'", manifestPath.string());
        return false;
    }
    manifest.preview.path = previewBlob->get<std::string>();
    if (!parseUvec3(*previewDimensions, manifest.preview.dimensions) ||
        !isNonZeroDimensions(manifest.preview.dimensions)) {
        errorOut = fmt::format("Invalid preview_dimensions in '{}'", manifestPath.string());
        return false;
    }
    if (!previewByteSize->is_number_unsigned()) {
        errorOut = fmt::format("Invalid preview_byte_size in '{}'", manifestPath.string());
        return false;
    }
    const auto parsedPreviewSize = previewByteSize->get<uint64_t>();
    if (parsedPreviewSize > std::numeric_limits<size_t>::max()) {
        errorOut = fmt::format("preview_byte_size too large in '{}'", manifestPath.string());
        return false;
    }
    manifest.preview.byteSize = static_cast<size_t>(parsedPreviewSize);

    if (!fullBlob->is_string()) {
        errorOut = fmt::format("Invalid full_blob in '{}'", manifestPath.string());
        return false;
    }
    manifest.full.path = fullBlob->get<std::string>();
    if (!parseUvec3(*fullDimensions, manifest.full.dimensions) ||
        !isNonZeroDimensions(manifest.full.dimensions)) {
        errorOut = fmt::format("Invalid full_dimensions in '{}'", manifestPath.string());
        return false;
    }
    if (!fullByteSize->is_number_unsigned()) {
        errorOut = fmt::format("Invalid full_byte_size in '{}'", manifestPath.string());
        return false;
    }
    const auto parsedFullSize = fullByteSize->get<uint64_t>();
    if (parsedFullSize > std::numeric_limits<size_t>::max()) {
        errorOut = fmt::format("full_byte_size too large in '{}'", manifestPath.string());
        return false;
    }
    manifest.full.byteSize = static_cast<size_t>(parsedFullSize);

    if (auto it = root.find("full_downsampled_from"); it != root.end()) {
        glm::uvec3 downsampledFrom{};
        if (!parseUvec3(*it, downsampledFrom) || !isNonZeroDimensions(downsampledFrom)) {
            errorOut = fmt::format("Invalid full_downsampled_from in '{}'", manifestPath.string());
            return false;
        }
        manifest.fullDownsampledFrom = downsampledFrom;
    }

    const auto baseDir = manifestPath.parent_path();
    manifest.preview.path = baseDir / manifest.preview.path;
    manifest.full.path = baseDir / manifest.full.path;

    if (manifest.datasetId.empty()) {
        errorOut = fmt::format("dataset_id must not be empty in '{}'", manifestPath.string());
        return false;
    }

    manifestOut = std::move(manifest);
    return true;
}
