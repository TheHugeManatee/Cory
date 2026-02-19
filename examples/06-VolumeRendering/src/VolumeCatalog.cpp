#include "VolumeCatalog.hpp"

#include <fmt/format.h>

#include <cctype>
#include <fstream>
#include <string_view>

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

} // namespace

bool loadVolumeCatalog(const std::filesystem::path &catalogPath,
                       VolumeCatalog &catalogOut,
                       std::string &errorOut)
{
    std::ifstream in(catalogPath);
    if (!in.is_open()) {
        errorOut = fmt::format("Failed to open volume catalog '{}'", catalogPath.string());
        return false;
    }

    std::string line;
    size_t lineNo = 0;
    bool seenVersion = false;
    std::vector<VolumeCatalogEntry> entries;
    const auto baseDir = catalogPath.parent_path();
    while (std::getline(in, line)) {
        ++lineNo;
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.starts_with('#')) {
            continue;
        }

        const auto equalsPos = trimmed.find('=');
        if (equalsPos == std::string::npos) {
            errorOut = fmt::format(
                "Invalid line {} in '{}': expected key=value", lineNo, catalogPath.string());
            return false;
        }
        auto key = trim(std::string_view{trimmed}.substr(0, equalsPos));
        auto value = trim(std::string_view{trimmed}.substr(equalsPos + 1));
        if (key.empty() || value.empty()) {
            errorOut = fmt::format(
                "Invalid line {} in '{}': empty key or value", lineNo, catalogPath.string());
            return false;
        }

        if (key == "cory_volume_catalog_version") {
            if (value != "1") {
                errorOut = fmt::format("Unsupported cory_volume_catalog_version '{}' in '{}'",
                                       value,
                                       catalogPath.string());
                return false;
            }
            seenVersion = true;
            continue;
        }
        if (key == "dataset") {
            entries.push_back(
                VolumeCatalogEntry{.manifestPath = baseDir / std::filesystem::path{value}});
            continue;
        }
        // unknown keys are ignored on purpose for forward compatibility
    }

    if (!seenVersion) {
        errorOut = fmt::format("Missing required cory_volume_catalog_version in '{}'",
                               catalogPath.string());
        return false;
    }
    if (entries.empty()) {
        errorOut =
            fmt::format("Volume catalog '{}' contains no dataset entries", catalogPath.string());
        return false;
    }

    catalogOut = VolumeCatalog{
        .catalogPath = catalogPath,
        .entries = std::move(entries),
    };
    return true;
}
