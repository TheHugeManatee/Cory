#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/Concepts.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/SceneGraph/Common.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>

#include <entt/entt.hpp>

namespace Cory {

template <typename T>
concept System = requires(T sys, SceneGraph &graph, TickInfo tickInfo, Entity entity) {
    {
        // for now, all we require is that a system has a tick function
        sys.tick(graph, tickInfo)
    } -> std::same_as<void>;
};

template <typename Sys>
concept SystemHasBeforeUpdate = requires(Sys sys, SceneGraph &graph) { sys.beforeUpdate(graph); };
template <typename Sys>
concept SystemHasAfterUpdate = requires(Sys sys, SceneGraph &graph) { sys.afterUpdate(graph); };

/**
 * @brief A basic system just monitors the scene graph and reacts to specific components
 *
 * This is a CRTP class that provides a simple interface for systems to operate on the scene graph.
 */
template <typename Derived, Component... Cmps> // CRTP
class BasicSystem {
  public:
    void tick(SceneGraph &graph, TickInfo tickInfo)
    {
        if constexpr (SystemHasBeforeUpdate<Derived>) {
            static_cast<Derived *>(this)->beforeUpdate(graph);
        }

        auto view = graph.registry().template view<Cmps...>();

        view.each([&](Entity entity, Cmps &...components) {
            static_cast<Derived *>(this)->update(graph, tickInfo, entity, components...);
        });

        if constexpr (SystemHasAfterUpdate<Derived>) {
            static_cast<Derived *>(this)->afterUpdate(graph);
        }
    }

  protected: // API functions for derived classes
    template <Component... Cmps_, CallableWith<Entity, Cmps_...> Func>
    void forEach(SceneGraph &graph, Func &&callable)
    {
        auto view = graph.registry().template view<Cmps...>();

        view.each(std::forward<Func>(callable));
    }
};

template <typename... Components>
class CallbackSystem : public BasicSystem<CallbackSystem<Components...>, Components...> {
  public:
    template <typename Fn>
    CallbackSystem(Fn &&fn)
        : updateFn_(std::forward<Fn>(fn))
    {
    }

    void update(SceneGraph &graph, TickInfo tickInfo, Entity entity, Components &...components)
    {
        updateFn_(graph, tickInfo, entity, components...);
    }

  private:
    std::function<void(SceneGraph &, TickInfo, Entity, Components &...)> updateFn_;
};

/** ====================================== Implementation ====================================== **/

} // namespace Cory