#include <Cory/Ecs/SystemScheduler.hpp>

#include <functional>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <utility>

namespace Cory::Ecs {
namespace {

using AccessMap = std::map<ResourceId, Access>;

Access strongestAccess(Access lhs, Access rhs)
{
    if (lhs == Access::Write || rhs == Access::Write) {
        return Access::Write;
    }
    return Access::Read;
}

AccessMap effectiveAccesses(const SystemDescription &description)
{
    AccessMap result;
    for (const ResourceAccess &access : description.accesses) {
        auto [it, inserted] = result.emplace(access.resource, access.access);
        if (!inserted) {
            it->second = strongestAccess(it->second, access.access);
        }
    }
    return result;
}

bool conflicts(Access previous, Access current)
{
    return previous == Access::Write || current == Access::Write;
}

} // namespace

SystemHandle SystemScheduler::registerSystem(SystemDescription description, SystemFn fn)
{
    const SystemHandle handle{systems_.size()};
    if (description.name.empty()) {
        description.name = "System " + std::to_string(handle.index);
    }
    systems_.push_back(SystemEntry{std::move(description), std::move(fn)});
    scheduleDirty_ = true;
    return handle;
}

void SystemScheduler::clear()
{
    systems_.clear();
    executionOrder_.clear();
    executionBatches_.clear();
    dependencyEdges_.clear();
    scheduleDirty_ = false;
}

void SystemScheduler::rebuildSchedule()
{
    dependencyEdges_.clear();
    executionOrder_.clear();
    executionBatches_.clear();

    const size_t systemCount = systems_.size();
    std::vector<std::set<size_t>> outgoing(systemCount);
    std::vector<size_t> incomingCount(systemCount, 0);

    struct ResourceState {
        std::vector<std::pair<SystemHandle, Access>> readers;
        std::optional<std::pair<SystemHandle, Access>> writer;
    };
    std::map<ResourceId, ResourceState> resourceStates;

    auto addEdge = [&](SystemHandle before,
                       SystemHandle after,
                       const ResourceId &resource,
                       Access beforeAccess,
                       Access afterAccess) {
        if (before == after) {
            return;
        }
        auto [_, inserted] = outgoing[before.index].insert(after.index);
        if (inserted) {
            ++incomingCount[after.index];
        }
        dependencyEdges_.push_back(
            DependencyEdge{before, after, resource, beforeAccess, afterAccess});
    };

    for (size_t systemIndex = 0; systemIndex < systemCount; ++systemIndex) {
        const SystemHandle current{systemIndex};
        const AccessMap accesses = effectiveAccesses(systems_[systemIndex].description);

        for (const auto &[resource, currentAccess] : accesses) {
            ResourceState &state = resourceStates[resource];
            if (state.writer.has_value() && conflicts(state.writer->second, currentAccess)) {
                addEdge(
                    state.writer->first, current, resource, state.writer->second, currentAccess);
            }

            if (currentAccess == Access::Write) {
                for (const auto &[reader, readerAccess] : state.readers) {
                    addEdge(reader, current, resource, readerAccess, currentAccess);
                }
                state.readers.clear();
                state.writer = std::pair{current, currentAccess};
            }
            else {
                state.readers.push_back(std::pair{current, currentAccess});
            }
        }
    }

    std::priority_queue<size_t, std::vector<size_t>, std::greater<>> ready;
    for (size_t i = 0; i < systemCount; ++i) {
        if (incomingCount[i] == 0) {
            ready.push(i);
        }
    }

    while (!ready.empty()) {
        const size_t batchSize = ready.size();
        std::vector<SystemHandle> batch;
        batch.reserve(batchSize);

        std::vector<size_t> currentBatch;
        currentBatch.reserve(batchSize);
        for (size_t i = 0; i < batchSize; ++i) {
            currentBatch.push_back(ready.top());
            ready.pop();
        }

        for (const size_t systemIndex : currentBatch) {
            executionOrder_.push_back(SystemHandle{systemIndex});
            batch.push_back(SystemHandle{systemIndex});

            for (const size_t dependent : outgoing[systemIndex]) {
                --incomingCount[dependent];
                if (incomingCount[dependent] == 0) {
                    ready.push(dependent);
                }
            }
        }

        executionBatches_.push_back(std::move(batch));
    }

    if (executionOrder_.size() != systemCount) {
        std::ostringstream message;
        message << "ECS system dependency graph contains a cycle; scheduled "
                << executionOrder_.size() << " of " << systemCount << " systems";
        throw ScheduleCycleError{message.str()};
    }

    scheduleDirty_ = false;
}

void SystemScheduler::execute()
{
    ensureScheduleCurrent();
    for (const SystemHandle handle : executionOrder_) {
        if (systems_[handle.index].fn) {
            systems_[handle.index].fn();
        }
    }
}

const SystemDescription &SystemScheduler::description(SystemHandle handle) const
{
    return systems_.at(handle.index).description;
}

const std::vector<SystemHandle> &SystemScheduler::executionOrder() const
{
    const_cast<SystemScheduler *>(this)->ensureScheduleCurrent();
    return executionOrder_;
}

const std::vector<std::vector<SystemHandle>> &SystemScheduler::executionBatches() const
{
    const_cast<SystemScheduler *>(this)->ensureScheduleCurrent();
    return executionBatches_;
}

const std::vector<DependencyEdge> &SystemScheduler::dependencyEdges() const
{
    const_cast<SystemScheduler *>(this)->ensureScheduleCurrent();
    return dependencyEdges_;
}

void SystemScheduler::ensureScheduleCurrent()
{
    if (scheduleDirty_) {
        rebuildSchedule();
    }
}

} // namespace Cory::Ecs
