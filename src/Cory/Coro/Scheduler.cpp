#include "Scheduler.hpp"

namespace Cory {

static_assert(Scheduler<SyncScheduler>, "SyncScheduler must satisfy the Scheduler concept");
static_assert(Scheduler<NextTickScheduler>, "NextTickScheduler must satisfy the Scheduler concept");

void NextTickScheduler::tick()
{
    std::vector<cppcoro::coroutine_handle<>> toResume = scheduled_.exchange({});

    for (const auto handle : toResume) {
        if (handle) {
            handle.resume();
        }
    }
}
} // namespace Cory