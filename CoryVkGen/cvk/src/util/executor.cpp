#include <cvk/util/executor.h>

#include <cvk/log.h>

#include <algorithm>
#include <numeric>


#if WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <processthreadsapi.h>
#endif

namespace cvk::util {

void executor::executor_main()
{
    set_thread_name();

    std::unique_lock lck{queue_mtx_};
    while (!stop_thread_) {
        drain_queue(lck);
        // wait until either stop is requested or queue has new entries
        queue_condition_.wait(lck, [&]() { return stop_thread_ || !task_queue_.empty(); });
    }

    // when stopping, drain the queue to make sure all tasks have been completed
    drain_queue(lck);
}

void executor::set_thread_name()
{
#if WIN32
    size_t convertedChars = 0;
    const size_t newsize = (name_.size() + 1) * 2;
    std::wstring wname;
    wname.resize(newsize);
    mbstowcs_s(&convertedChars, wname.data(), name_.size() + 1, name_.c_str(), _TRUNCATE);
    SetThreadDescription(GetCurrentThread(), wname.c_str());
#endif
}

void executor::drain_queue(std::unique_lock<std::mutex> &lck)
{
    while (!task_queue_.empty()) {
        // dequeue a task from the queue
        // move the task out of the queue and pop it
        std::packaged_task<void()> task{std::move(task_queue_.front())};
        task_queue_.pop();
        {
            lck.unlock();
            try {
                task();
            }
            catch (...) {
                // ignore all exceptions to ensure the lock is re-acquired
            }
            lck.lock();
        }
    }
}

void executor::flush()
{
    CVK_ASSERT(std::this_thread::get_id() != worker_thread_.get_id(),
                   "cannot flush from the executor worker thread!");
    // enqueue an empty marker job and wait for it to be executed
    async([]() {}).get();
}

} // namespace cory::utils
