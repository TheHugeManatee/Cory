#include <Cory/Systems/SystemCoordinator.hpp>

namespace Cory {


void SystemCoordinator::tick(SceneGraph &graph, TickInfo tickInfo)
{
    for (auto &sys : systems_) {
        sys->tick(graph, tickInfo);
    }
}

} // namespace Cory