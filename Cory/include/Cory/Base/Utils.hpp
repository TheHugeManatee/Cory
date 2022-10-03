#pragma once

#include "stb_image.h"
#include <cstdint>

#include <cmath>
#include <filesystem>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace Cory {

/// create a human-readable string for a byte size, e.g. something like "1.52 MiB"
std::string formatBytes(size_t bytes);

/// read the whole contents of a file - no memory mapping etc. applied here!
std::vector<char> readFile(const std::filesystem::path &filename);



struct stbi_image {
  public:
    explicit stbi_image(const std::string &file);
    ~stbi_image();
    size_t size() { return size_t(width) * size_t(height) * 4; }

    int width{};
    int height{};
    int channels{};
    unsigned char *data{};
};

} // namespace Cory