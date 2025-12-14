#pragma once

#include <expected>
#include <string>

namespace Cory {

template <typename T> using Result = std::expected<T, std::string>;

}