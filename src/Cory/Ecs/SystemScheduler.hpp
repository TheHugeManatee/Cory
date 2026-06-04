#pragma once

#include <Cory/Ecs/Common.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace Cory::Ecs {

struct SystemHandle {
    size_t index{static_cast<size_t>(-1)};

    friend bool operator==(SystemHandle, SystemHandle) = default;
};

struct SystemDescription {
    std::string name;
    std::vector<ResourceAccess> accesses;
};

struct DependencyEdge {
    SystemHandle before;
    SystemHandle after;
    ResourceId resource;
    Access beforeAccess{Access::Read};
    Access afterAccess{Access::Read};
};

class ScheduleCycleError : public std::runtime_error {
  public:
    explicit ScheduleCycleError(const std::string &message)
        : std::runtime_error(message)
    {
    }
};

class SystemScheduler {
  public:
    SystemHandle registerSystem(SystemDescription description, SystemFn fn);

    void clear();
    void rebuildSchedule();
    void execute();

    [[nodiscard]] bool scheduleDirty() const noexcept { return scheduleDirty_; }
    [[nodiscard]] const SystemDescription &description(SystemHandle handle) const;
    [[nodiscard]] const std::vector<SystemHandle> &executionOrder() const;
    [[nodiscard]] const std::vector<std::vector<SystemHandle>> &executionBatches() const;
    [[nodiscard]] const std::vector<DependencyEdge> &dependencyEdges() const;

  private:
    struct SystemEntry {
        SystemDescription description;
        SystemFn fn;
    };

    void ensureScheduleCurrent();

    std::vector<SystemEntry> systems_;
    std::vector<SystemHandle> executionOrder_;
    std::vector<std::vector<SystemHandle>> executionBatches_;
    std::vector<DependencyEdge> dependencyEdges_;
    bool scheduleDirty_{false};
};

} // namespace Cory::Ecs
