#include <Cory/Base/CoroThreadPool.hpp>

#include <algorithm>
#include <cassert>

namespace Cory {

CoroThreadPool::CoroThreadPool(size_t workerCount)
{
    const auto effectiveWorkerCount = std::max<size_t>(1u, workerCount);
    workers_.reserve(effectiveWorkerCount);
    for (size_t i = 0; i < effectiveWorkerCount; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
}

CoroThreadPool::~CoroThreadPool()
{
    {
        auto lock = std::scoped_lock{queueMutex_};
        stopRequested_ = true;
    }
    queueCv_.notify_all();

    for (auto &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void CoroThreadPool::ScheduleAwaiter::await_suspend(
    cppcoro::coroutine_handle<> handle) const noexcept
{
    if (pool != nullptr) {
        pool->enqueue(handle);
    }
}

void CoroThreadPool::enqueue(cppcoro::coroutine_handle<> handle) noexcept
{
    {
        auto lock = std::scoped_lock{queueMutex_};
        assert(!stopRequested_ && "CoroThreadPool::enqueue called after shutdown");
        queue_.push_back(handle);
    }
    queueCv_.notify_one();
}

void CoroThreadPool::workerLoop()
{
    while (true) {
        auto next = cppcoro::coroutine_handle<>{};

        {
            auto lock = std::unique_lock{queueMutex_};
            queueCv_.wait(lock, [this] { return stopRequested_ || !queue_.empty(); });
            if (queue_.empty()) {
                if (stopRequested_) {
                    return;
                }
                continue;
            }

            next = queue_.front();
            queue_.pop_front();
        }

        if (next) {
            next.resume();
        }
    }
}

} // namespace Cory
