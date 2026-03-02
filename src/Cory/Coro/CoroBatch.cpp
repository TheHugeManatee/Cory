#include "CoroBatch.hpp"

#include <fmt/format.h>

#include <cppcoro/when_all_ready.hpp>

#include <exception>
#include <utility>
#include <vector>

namespace Cory {
namespace {

cppcoro::task<Result<void>> wrapTaskForFailFast(cppcoro::task<Result<void>> task,
                                                std::stop_source *stopSource)
{
    try {
        auto result = co_await std::move(task);
        if (!result) {
            if (stopSource != nullptr) {
                stopSource->request_stop();
            }
            co_return std::unexpected(std::move(result.error()));
        }
        co_return Result<void>{};
    }
    catch (const std::exception &exception) {
        if (stopSource != nullptr) {
            stopSource->request_stop();
        }
        co_return std::unexpected(
            fmt::format("Unhandled exception while waiting on task group: {}", exception.what()));
    }
    catch (...) {
        if (stopSource != nullptr) {
            stopSource->request_stop();
        }
        co_return std::unexpected("Unhandled unknown exception while waiting on task group");
    }
}

} // namespace

cppcoro::task<Result<void>> when_all_fail_fast(std::vector<cppcoro::task<Result<void>>> tasks,
                                               std::stop_source &stopSource)
{
    if (tasks.empty()) {
        co_return Result<void>{};
    }

    auto wrappedTasks = std::vector<cppcoro::task<Result<void>>>{};
    wrappedTasks.reserve(tasks.size());

    for (auto &task : tasks) {
        wrappedTasks.emplace_back(wrapTaskForFailFast(std::move(task), &stopSource));
    }

    auto completed = co_await cppcoro::when_all_ready(std::move(wrappedTasks));
    for (auto &task : completed) {
        auto result = task.result();
        if (!result) {
            co_return std::unexpected(std::move(result.error()));
        }
    }

    co_return Result<void>{};
}

} // namespace Cory
