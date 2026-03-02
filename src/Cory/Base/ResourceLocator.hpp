//
// Created by j on 10/2/2022.
//

#pragma once

#include <Cory/Base/Result.hpp>

#include <filesystem>
#include <vector>

namespace Cory {

enum class ResourceType {
    Shader,
    Texture,
    Model,
    Any,
};

class ResourceLocator {
  public:
    /// add a search path for resources. the path will be appended at the end of all paths
    static void addSearchPath(std::filesystem::path path);

    /**
     * Locate a path/file by checking all resource search paths and returning the full path that
     * matches the file. most recently added search paths will be checked first
     *
     * @param type The resource type
     */
    static Result<std::filesystem::path> Locate(std::filesystem::path resourcePath,
                                                ResourceType type = ResourceType::Any);

  private:
    static std::vector<std::filesystem::path> &searchPaths();
};

} // namespace Cory
