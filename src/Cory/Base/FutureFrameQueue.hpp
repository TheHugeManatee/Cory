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
template <typename Time, std::copyable WaitingObject> class FutureFrameQueue {
  public:
    /// enqueue an object for the given frame
    template <typename T> void enqueueFor(Time time, T &&obj)
    {
        waitingObjects_.emplace(time, std::forward<T>(obj));
    }

    /// dequeue all objects waiting for the given time or any previous times
    std::vector<WaitingObject> dequeueUntil(Time time);

    /// dequeue all objects
    std::vector<WaitingObject> dequeueAll();

    /// clear all objects
    void clear() { waitingObjects_.clear(); }
    auto size() { return waitingObjects_.size(); }

  private:
    std::multimap<Time, WaitingObject> waitingObjects_;
};

template <typename Time, std::copyable WaitingObject>
std::vector<WaitingObject> FutureFrameQueue<Time, WaitingObject>::dequeueAll()
{
    auto dequeued =
        ranges::views::all(waitingObjects_) | ranges::views::values | ranges::to<std::vector>();
    waitingObjects_.clear();
    return dequeued;
}

template <typename Time, std::copyable WaitingObject>
std::vector<WaitingObject> FutureFrameQueue<Time, WaitingObject>::dequeueUntil(Time time)
{
    auto bound = waitingObjects_.upper_bound(time);
    // wrap the range [begin, bound] into a view so we can use ranges::to<..>
    auto dequeued = ranges::subrange(waitingObjects_.begin(), bound) | ranges::views::values |
                    ranges::to<std::vector>();

    waitingObjects_.erase(waitingObjects_.begin(), bound);
    return dequeued;
}
} // namespace Cory
