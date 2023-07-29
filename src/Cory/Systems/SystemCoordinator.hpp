#pragma once

#include <Cory/SceneGraph/System.hpp>

#include <variant>

namespace Cory {

class SystemCoordinator {
  public:
    template <System Sys, typename... Args> Sys &emplace(Args &&...args);

    void tick(SceneGraph &graph, TickInfo tickInfo)
    {
        for (auto &sys : systems_) {
            sys->tick(graph, tickInfo);
        }
    }

    struct SystemBase {
        virtual ~SystemBase() = default;
        virtual void tick(SceneGraph &graph, TickInfo tickInfo) = 0;
    };

  private:
    std::vector<std::unique_ptr<SystemBase>> systems_;
};

/** ====================================== Implementation ====================================== **/

namespace {
template <System Sys> struct SystemImpl : public SystemCoordinator::SystemBase {
    template <typename... Args>
    SystemImpl(Args &&...args)
        : sys_{std::forward<Args>(args)...}
    {
    }
    void tick(SceneGraph &graph, TickInfo tickInfo) override { sys_.tick(graph, tickInfo); }
    Sys sys_;
};
} // namespace

template <System Sys, typename... Args> Sys &SystemCoordinator::emplace(Args &&...args)
{

    // we have to wrap the
    auto sys = std::make_unique<SystemImpl<Sys>>(std::forward<Args>(args)...);
    Sys &sysref = sys->sys_;
    systems_.emplace_back(std::move(sys));

    return sysref;
}

} // namespace Cory