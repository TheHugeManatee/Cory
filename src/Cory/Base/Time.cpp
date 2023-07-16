#include "Cory/Base/Time.hpp"

namespace Cory {

// static variable init
AppClock::upstream_clock::duration AppClock::epoch_;
AppClock::system_clock::duration AppClock::systemEpoch_;

void AppClock::Init()
{
    epoch_ = upstream_clock::now().time_since_epoch();
    systemEpoch_ = system_clock::now().time_since_epoch();
}

} // namespace Cory