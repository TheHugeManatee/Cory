#pragma once

#include <concepts>
#include <cstdint>
#include <map>
#include <vector>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/subrange.hpp>

namespace Cory {
/**
 * utility class to queue up things that should be processed on a specific frame in the future
 */
template <std::copyable WaitingObject> class FutureFrameQueue {
  public:
    /// enqueue an object for the given frame
    template <typename T> void enqueueFor(uint64_t frame, T &&obj)
    {
        waitingObjects_.emplace(frame, std::forward<T>(obj));
    }

    /// dequeue all objects waiting for the given frame or any previous
    std::vector<WaitingObject> dequeueUntil(uint64_t frame);

    /// dequeue all objects
    std::vector<WaitingObject> dequeueAll();

    /// clear all objects
    void clear() { waitingObjects_.clear(); }
    auto size() { return waitingObjects_.size(); }

  private:
    std::multimap<uint64_t, WaitingObject> waitingObjects_;
};

template <std::copyable WaitingObject>
std::vector<WaitingObject> FutureFrameQueue<WaitingObject>::dequeueAll()
{
    auto dequeued =
        ranges::views::all(waitingObjects_) | ranges::views::values | ranges::to<std::vector>();
    waitingObjects_.clear();
    return dequeued;
}

template <std::copyable WaitingObject>
std::vector<WaitingObject> FutureFrameQueue<WaitingObject>::dequeueUntil(uint64_t frame)
{
    auto bound = waitingObjects_.upper_bound(frame);
    // wrap the range [begin, bound] into a view so we can use ranges::to<..>
    auto dequeued = ranges::subrange(waitingObjects_.begin(), bound) | ranges::views::values |
                    ranges::to<std::vector>();
    
    waitingObjects_.erase(waitingObjects_.begin(), bound);
    return dequeued;
}
} // namespace Cory
