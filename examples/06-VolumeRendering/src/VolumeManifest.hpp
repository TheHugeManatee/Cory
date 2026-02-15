#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

struct VolumeBlobInfo {
    std::filesystem::path path{};
    glm::uvec3 dimensions{0u};
    size_t byteSize{0};
};

struct VolumeBmpStackInfo {
    std::filesystem::path directory{};
    std::string pattern{"*.bmp"};
    size_t maxConcurrency{0};
};

struct VolumeManifest {
    std::filesystem::path manifestPath{};
    std::string datasetId{};
    glm::vec3 spacingMm{1.0f};
    glm::uvec3 sourceDimensions{0u};
    VolumeBlobInfo preview{};
    VolumeBlobInfo full{};
    std::optional<VolumeBmpStackInfo> bmpStack{};
    std::optional<glm::uvec3> fullDownsampledFrom{};
    std::string normalization{};
};

[[nodiscard]] bool loadVolumeManifest(const std::filesystem::path &manifestPath,
                                      VolumeManifest &manifestOut,
                                      std::string &errorOut);
