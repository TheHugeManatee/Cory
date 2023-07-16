#include <Cory/Base/ResourceLocator.hpp>

#include <Cory/Base/Log.hpp>

#include <fmt/core.h>

namespace Cory {

// seeded with the empty path, i.e. relative to the current working directory or an absolute path
std::vector<std::filesystem::path> ResourceLocator::searchPaths_{CORY_RESOURCE_DIR, ""};

void ResourceLocator::addSearchPath(std::filesystem::path path)
{
    CO_CORE_INFO("ResourceLocator: Adding search path: {}", path.string());
    searchPaths_.insert(searchPaths_.begin(), path);
}

std::filesystem::path ResourceLocator::Locate(std::filesystem::path resourcePath)
{
    for (const auto &searchPath : searchPaths_) {
        auto combined = searchPath / resourcePath;
        if (exists(combined)) { return absolute(combined); }
    }
    throw ResourceNotFound{fmt::format("Resource could not be found: {}", resourcePath.string())};
}

} // namespace Cory