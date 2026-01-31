#include <Cory/Base/ResourceLocator.hpp>

#include <Cory/Base/Log.hpp>

#include <fmt/core.h>

namespace Cory {

ResourceNotFound::~ResourceNotFound() = default;

void ResourceLocator::addSearchPath(std::filesystem::path path)
{
    CO_CORE_INFO("ResourceLocator: Adding search path: {}", path.string());
    searchPaths().insert(searchPaths().begin(), path);
}

std::filesystem::path ResourceLocator::Locate(std::filesystem::path resourcePath,
                                              [[maybe_unused]] ResourceType type)
{
    for (const auto &searchPath : searchPaths()) {
        auto combined = searchPath / resourcePath;
        if (exists(combined)) {
            return absolute(combined);
        }
    }
    throw ResourceNotFound{fmt::format("Resource could not be found: {}", resourcePath.string())};
}

} // namespace Cory

std::vector<std::filesystem::path> &Cory::ResourceLocator::searchPaths()
{
    static std::vector<std::filesystem::path> *paths =
        new std::vector<std::filesystem::path>{CORY_DATA_DIR, ""};
    return *paths;
}
