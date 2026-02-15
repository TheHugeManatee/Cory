#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct VolumeCatalogEntry {
    std::filesystem::path manifestPath{};
};

struct VolumeCatalog {
    std::filesystem::path catalogPath{};
    std::vector<VolumeCatalogEntry> entries{};
};

[[nodiscard]] bool loadVolumeCatalog(const std::filesystem::path &catalogPath,
                                     VolumeCatalog &catalogOut,
                                     std::string &errorOut);
