#include "Properties.hpp"

#include "PropertySet.hpp"

namespace Cory::Proper {

void AbstractProperty::registerWaiter(cppcoro::coroutine_handle<> waiter)
{
    waiters_.push_back(waiter);
}

void AbstractProperty::unregisterWaiter(cppcoro::coroutine_handle<> waiter)
{
    std::erase(waiters_, waiter);
}

void AbstractProperty::notifyWaiters()
{
    // Waiters are resumed once per notification - if the waiting coroutines are
    // interested in subsequent notifications, they are responsible for re-registering themselves.
    auto waiters = waiters_;
    // Perf note: We keep the existing waiters_ cleared, but intentionally keep its
    // capacity the same - in the average case, the same waiters will be immediately re-registered
    // from within the resumed coroutines
    waiters_.clear();
    for (auto waiter : waiters) {
        waiter.resume();
    }
    waiters_.shrink_to_fit();
}

void Task<void>::promise_type::cancel()
{
    if (cancelProperty_ && cancelAwaiting_) {
        cancelProperty_->unregisterWaiter(cancelAwaiting_);
    }
    clearCancellation();
}

} // namespace Cory::Proper
