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

  private:
    struct SystemBase {
        virtual ~SystemBase() = default;
        virtual void tick(SceneGraph &graph, TickInfo tickInfo) = 0;
    };

    template <System Sys> struct SystemImpl : public SystemBase {
        SystemImpl(Sys sys)
            : sys_{std::move(sys)}
        {
        }
        void tick(SceneGraph &graph, TickInfo tickInfo) override { sys_.tick(graph, tickInfo); }
        Sys sys_;
    };

    std::vector<std::unique_ptr<SystemBase>> systems_;
};

/** ====================================== Implementation ====================================== **/

template <System Sys, typename... Args> Sys &SystemCoordinator::emplace(Args &&...args)
{
    // we have to wrap the
    auto sys = std::make_unique<SystemImpl<Sys>>(std::forward<Args>(args)...);
    Sys &sysref = sys->sys_;
    systems_.emplace_back(std::move(sys));

    return sysref;
}

} // namespace Cory