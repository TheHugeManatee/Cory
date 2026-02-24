#include "Properties.hpp"

#include "PropertySet.hpp"

namespace Cory::Prop::Proper {

void Task<void>::promise_type::cancel()
{
    if (cancelSet_ && cancelAwaiting_) {
        cancelSet_->unregisterWaiter(cancelProperty_, cancelAwaiting_);
    }
    clearCancellation();
}

} // namespace Cory::Prop::Proper
