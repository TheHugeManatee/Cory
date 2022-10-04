//
// Created by j on 10/2/2022.
//

#pragma once

#include <filesystem>
#include <stdexcept>

namespace Cory {

struct ResourceNotFound : public std::runtime_error {
    ResourceNotFound(std::string message)
        : std::runtime_error(std::move(message)){};
};

class ResourceLocator {
  public:
    /// add a search path for resources. the path will be appended at the end of all paths
    static void addSearchPath(std::filesystem::path path);

    /**
     * locate a path/file by checking all resource search paths and returning the full path that
     * matches the file. most recently added search paths will be checked first
     *
     * @throws ResourceNotFound if a resource cannot be located
     */
    static std::filesystem::path locate(std::filesystem::path resourcePath);

  private:
    static std::vector<std::filesystem::path> searchPaths_;
};

} // namespace Cory
