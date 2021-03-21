#pragma once

#include "Cory/types.h"

#include <condition_variable>
#include <future>
#include <queue>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

namespace cory::utils {

/**
 * very simple task queue that spawns a single worker thread that will asynchronously execute the
 * tasks in order.
 */
class executor {
  public:
    executor(std::string name)
        : name_{std::move(name)}
    {
        // start the worker thread
        worker_thread_ = std::thread([this]() { executor_main(); });
    }

    /**
     * will drain the event queue. no idea what's going to happen if you try to schedule more tasks
     * while the executor is destroying, so let's call it undefined behavior (just don't).
     */
    ~executor()
    {
        stop_thread_ = true;
        queue_condition_.notify_one();

        if (worker_thread_.joinable()) worker_thread_.join();
    }

    [[nodiscard]] std::string_view name() const noexcept { return name_; }

    /**
     * drain the queue and force all tasks that were executed so far to be executed. does not wait
     * for tasks might be submitted concurrently to this method.
     */
    void flush();

    /**
     * schedule a task to be executed. currently only `void()` tasks are supported.
     *
     * @param task the task (usually a lambda or a void() function reference) to schedule.
     * @return a future<void> that you may wait for if you are interested in when the task is
     * finished.
     */
    template <typename TaskLambda> auto async(TaskLambda &&task)
    {
        using ResultType = decltype(task());
        static_assert(std::is_same_v<ResultType, void>,
                      "you must provide a callable that returns a void!");

        if constexpr (std::is_same_v<decltype(task()), void>) {
            std::packaged_task p_task(task);
            auto task_future = p_task.get_future();
            {
                std::unique_lock lck{queue_mtx_};
                task_queue_.push(std::move(p_task));
            }
            queue_condition_.notify_one();
            return task_future;
        }
        else {
            throw std::logic_error("The method or operation is not implemented.");
            return std::future<ResultType>{};
        }
    }

  private:
    /**
     * @brief worker thread entry point
     */
    void executor_main();

    void set_thread_name();

    /**
     * @brief internal method to drain the queue completely. locks the queue only temporarily, so
     * new tasks may be enqueued while the queue is draining
     */
    void drain_queue(std::unique_lock<std::mutex> &lck);

  private:
    std::atomic_bool stop_thread_{false};
    std::thread worker_thread_;
    std::string name_;
    std::queue<std::packaged_task<void()>> task_queue_;
    std::condition_variable queue_condition_;
    std::mutex queue_mtx_;
};

} // namespace cory::utils