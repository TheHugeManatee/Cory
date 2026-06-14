#pragma once

#include <Cory/Base/Result.hpp>

#include <cppcoro/task.hpp>

#include <stop_token>
#include <vector>

namespace Cory {

[[nodiscard]] cppcoro::task<Result<void>>
when_all_fail_fast(std::vector<cppcoro::task<Result<void>>> tasks, std::stop_source &stopSource);

} // namespace Cory
