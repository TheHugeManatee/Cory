#include <Cory/Renderer/ThreadScheduler.hpp>

#include <Cory/Base/Log.hpp>

namespace Cory {

ThreadScheduler::ThreadScheduler()
    : ownerThreadId_{std::this_thread::get_id()}
{
}

bool ThreadScheduler::isCurrentThread() const noexcept
{
    return std::this_thread::get_id() == ownerThreadId_;
}

void ThreadScheduler::assertCurrentThread(char const *methodName) const
{
    CO_CORE_ASSERT(isCurrentThread(),
                   "ThreadScheduler::{} must be called from the owner thread.",
                   methodName);
}

bool ThreadScheduler::ScheduleAwaiter::await_ready() const noexcept
{
    return scheduler == nullptr || scheduler->isCurrentThread();
}

void ThreadScheduler::ScheduleAwaiter::await_suspend(cppcoro::coroutine_handle<> handle) const noexcept
{
    if (scheduler != nullptr) {
        scheduler->enqueue(handle);
    }
}

void ThreadScheduler::enqueue(cppcoro::coroutine_handle<> handle) noexcept
{
    std::scoped_lock lock(queueMutex_);
    queue_.push_back(handle);
}

void ThreadScheduler::poll()
{
    assertCurrentThread("poll");

    while (true) {
        auto queuedResumes = std::deque<cppcoro::coroutine_handle<>>{};
        {
            std::scoped_lock lock(queueMutex_);
            if (queue_.empty()) {
                break;
            }
            queuedResumes.swap(queue_);
        }

        for (auto handle : queuedResumes) {
            if (handle) {
                handle.resume();
            }
        }
    }
}

} // namespace Cory
