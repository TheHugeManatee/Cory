#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/SceneGraph/Common.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>

#include <entt/entt.hpp>

namespace Cory {

/**
 * @brief A system is a coherent collection of logic that operates on the scene graph
 */
class System {
  public:
    virtual ~System() = default;
    virtual void tick(SceneGraph& graph, TickInfo tickInfo) = 0;
};

template <typename Derived, Component... Cmps> // CRTP
class SimpleSystem : public System {
  public:
    void tick(SceneGraph& graph, TickInfo tickInfo) override
    {
        static_cast<Derived *>(this)->beforeUpdate(graph);

        auto view = graph.registry().template view<Cmps...>();

        view.each([&](Entity entity, Cmps &...components) {
            static_cast<Derived *>(this)->update(graph, tickInfo, entity, components...);
        });

        static_cast<Derived *>(this)->afterUpdate(graph);
    }
};

/** ====================================== Implementation ====================================== **/

} // namespace Cory