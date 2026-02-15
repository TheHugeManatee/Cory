#include "VolumeManifest.hpp"

#include <Cory/Base/Log.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace {

std::string trim(std::string_view sv)
{
    size_t first = 0;
    while (first < sv.size() && std::isspace(static_cast<unsigned char>(sv[first])) != 0) {
        ++first;
    }
    size_t last = sv.size();
    while (last > first && std::isspace(static_cast<unsigned char>(sv[last - 1])) != 0) {
        --last;
    }
    return std::string{sv.substr(first, last - first)};
}

[[nodiscard]] bool parseUnsigned(std::string_view sv, uint32_t &valueOut)
{
    auto trimmed = trim(sv);
    if (trimmed.empty()) {
        return false;
    }
    uint32_t value = 0;
    const auto *begin = trimmed.data();
    const auto *end = begin + trimmed.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end) {
        return false;
    }
    valueOut = value;
    return true;
}

[[nodiscard]] bool parseSizeT(std::string_view sv, size_t &valueOut)
{
    auto trimmed = trim(sv);
    if (trimmed.empty()) {
        return false;
    }
    unsigned long long value = 0;
    const auto *begin = trimmed.data();
    const auto *end = begin + trimmed.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end || value > std::numeric_limits<size_t>::max()) {
        return false;
    }
    valueOut = static_cast<size_t>(value);
    return true;
}

[[nodiscard]] bool parseFloat(std::string_view sv, float &valueOut)
{
    auto trimmed = trim(sv);
    if (trimmed.empty()) {
        return false;
    }

    char *end = nullptr;
    const auto value = std::strtof(trimmed.c_str(), &end);
    if (end == nullptr || *end != '\0') {
        return false;
    }
    valueOut = value;
    return true;
}

template <typename T, size_t N>
[[nodiscard]] bool splitCommaValues(std::string_view sv, std::array<T, N> &valuesOut)
{
    size_t start = 0;
    size_t idx = 0;
    while (start <= sv.size()) {
        const size_t comma = sv.find(',', start);
        const auto token =
            sv.substr(start, comma == std::string_view::npos ? sv.size() - start : comma - start);
        if (idx >= N) {
            return false;
        }

        if constexpr (std::is_same_v<T, uint32_t>) {
            if (!parseUnsigned(token, valuesOut[idx])) {
                return false;
            }
        }
        else if constexpr (std::is_same_v<T, float>) {
            if (!parseFloat(token, valuesOut[idx])) {
                return false;
            }
        }
        else {
            static_assert(sizeof(T) == 0, "Unsupported splitCommaValues type");
        }

        ++idx;
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    return idx == N;
}

[[nodiscard]] bool parseUvec3(std::string_view sv, glm::uvec3 &valueOut)
{
    std::array<uint32_t, 3> values{};
    if (!splitCommaValues<uint32_t, 3>(sv, values)) {
        return false;
    }
    valueOut = glm::uvec3{values[0], values[1], values[2]};
    return true;
}

[[nodiscard]] bool parseVec3(std::string_view sv, glm::vec3 &valueOut)
{
    std::array<float, 3> values{};
    if (!splitCommaValues<float, 3>(sv, values)) {
        return false;
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

    std::unordered_map<std::string, std::string> values;
    std::string line;
    size_t lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.starts_with('#')) {
            continue;
        }

        const auto equalsPos = trimmed.find('=');
        if (equalsPos == std::string::npos) {
            errorOut = fmt::format(
                "Invalid line {} in '{}': expected key=value", lineNo, manifestPath.string());
            return false;
        }

        auto key = trim(std::string_view{trimmed}.substr(0, equalsPos));
        auto value = trim(std::string_view{trimmed}.substr(equalsPos + 1));
        if (key.empty() || value.empty()) {
            errorOut = fmt::format(
                "Invalid line {} in '{}': empty key or value", lineNo, manifestPath.string());
            return false;
        }
        values[std::move(key)] = std::move(value);
    }

    auto require = [&](const char *key) -> const std::string * {
        if (auto it = values.find(key); it != values.end()) {
            return &it->second;
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

    if (*version != "1") {
        errorOut = fmt::format("Unsupported cory_volume_manifest_version '{}' in '{}'",
                               *version,
                               manifestPath.string());
        return false;
    }
    if (*voxelFormat != "r8_unorm") {
        errorOut = fmt::format(
            "Unsupported voxel_format '{}' in '{}'", *voxelFormat, manifestPath.string());
        return false;
    }
    if (*endianness != "little") {
        errorOut =
            fmt::format("Unsupported endianness '{}' in '{}'", *endianness, manifestPath.string());
        return false;
    }

    VolumeManifest manifest{};
    manifest.manifestPath = manifestPath;
    manifest.datasetId = *datasetId;
    manifest.normalization = *normalization;

    if (!parseVec3(*spacing, manifest.spacingMm)) {
        errorOut = fmt::format("Invalid spacing_mm '{}' in '{}'", *spacing, manifestPath.string());
        return false;
    }
    if (!parseUvec3(*sourceDimensions, manifest.sourceDimensions)) {
        errorOut = fmt::format(
            "Invalid source_dimensions '{}' in '{}'", *sourceDimensions, manifestPath.string());
        return false;
    }

    manifest.preview.path = *previewBlob;
    if (!parseUvec3(*previewDimensions, manifest.preview.dimensions)) {
        errorOut = fmt::format(
            "Invalid preview_dimensions '{}' in '{}'", *previewDimensions, manifestPath.string());
        return false;
    }
    if (!parseSizeT(*previewByteSize, manifest.preview.byteSize)) {
        errorOut = fmt::format(
            "Invalid preview_byte_size '{}' in '{}'", *previewByteSize, manifestPath.string());
        return false;
    }

    manifest.full.path = *fullBlob;
    if (!parseUvec3(*fullDimensions, manifest.full.dimensions)) {
        errorOut = fmt::format(
            "Invalid full_dimensions '{}' in '{}'", *fullDimensions, manifestPath.string());
        return false;
    }
    if (!parseSizeT(*fullByteSize, manifest.full.byteSize)) {
        errorOut = fmt::format(
            "Invalid full_byte_size '{}' in '{}'", *fullByteSize, manifestPath.string());
        return false;
    }

    if (auto it = values.find("full_downsampled_from"); it != values.end()) {
        glm::uvec3 downsampledFrom{};
        if (!parseUvec3(it->second, downsampledFrom)) {
            errorOut = fmt::format(
                "Invalid full_downsampled_from '{}' in '{}'", it->second, manifestPath.string());
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

    for (const auto &[key, value] : values) {
        if (key == "cory_volume_manifest_version" || key == "dataset_id" || key == "voxel_format" ||
            key == "endianness" || key == "spacing_mm" || key == "source_dimensions" ||
            key == "preview_blob" || key == "preview_dimensions" || key == "preview_byte_size" ||
            key == "full_blob" || key == "full_dimensions" || key == "full_byte_size" ||
            key == "full_downsampled_from" || key == "normalization") {
            continue;
        }
        CO_CORE_WARN("Volume manifest '{}': unknown key '{}' ignored", manifestPath.string(), key);
    }

    return true;
}
