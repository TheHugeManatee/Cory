#pragma once

#include <future>

namespace cory {

template <typename T> using future = std::future<T>;

}