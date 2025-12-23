#include <Cory/Cory.hpp>

#include <Cory/Base/FileWatchManager.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/Base/Time.hpp>

namespace Cory {

void Init()
{
    // initialize all static objects in the correct order
    AppClock::Init();
    SimulationClock::Init();
    Log::Init();
    FileWatchManager::Init();
}

void Shutdown()
{
    FileWatchManager::Shutdown();
}

} // namespace Cory
