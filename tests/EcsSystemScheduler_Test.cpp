#include <Cory/Ecs/SystemScheduler.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace Cory::Ecs;

namespace {
struct Transform {};
struct Velocity {};
struct Health {};
struct Skeleton {};
} // namespace

TEST_CASE("ECS scheduler runs independent systems in registration order")
{
    SystemScheduler scheduler;
    std::vector<std::string> calls;

    const SystemHandle movement = scheduler.registerSystem(
        SystemDescription{.name = "Movement", .accesses = {read<Velocity>(), write<Transform>()}},
        [&] { calls.push_back("Movement"); });
    const SystemHandle damage =
        scheduler.registerSystem(SystemDescription{.name = "Damage", .accesses = {write<Health>()}},
                                 [&] { calls.push_back("Damage"); });

    scheduler.execute();

    CHECK(calls == std::vector<std::string>{"Movement", "Damage"});
    REQUIRE(scheduler.executionBatches().size() == 1);
    CHECK(scheduler.executionBatches()[0] == std::vector<SystemHandle>{movement, damage});
    CHECK(scheduler.dependencyEdges().empty());
}

TEST_CASE("ECS scheduler serializes read write component conflicts")
{
    SystemScheduler scheduler;
    std::vector<std::string> calls;

    const SystemHandle movement = scheduler.registerSystem(
        SystemDescription{.name = "Movement", .accesses = {read<Velocity>(), write<Transform>()}},
        [&] { calls.push_back("Movement"); });
    const SystemHandle animation = scheduler.registerSystem(
        SystemDescription{.name = "Animation", .accesses = {read<Transform>(), write<Skeleton>()}},
        [&] { calls.push_back("Animation"); });
    const SystemHandle renderPrep = scheduler.registerSystem(
        SystemDescription{.name = "RenderPrep", .accesses = {read<Transform>(), read<Skeleton>()}},
        [&] { calls.push_back("RenderPrep"); });

    scheduler.execute();

    CHECK(calls == std::vector<std::string>{"Movement", "Animation", "RenderPrep"});
    CHECK(scheduler.executionOrder() == std::vector<SystemHandle>{movement, animation, renderPrep});
    const auto &edges = scheduler.dependencyEdges();
    REQUIRE(edges.size() == 3);
    const auto containsEdge = [&](SystemHandle before, SystemHandle after, ResourceId resource) {
        return std::ranges::any_of(edges, [&](const DependencyEdge &edge) {
            return edge.before == before && edge.after == after && edge.resource == resource;
        });
    };
    CHECK(containsEdge(movement, animation, resource<Transform>()));
    CHECK(containsEdge(movement, renderPrep, resource<Transform>()));
    CHECK(containsEdge(animation, renderPrep, resource<Skeleton>()));
}

TEST_CASE("ECS scheduler exposes parallel batches for independent predecessors")
{
    SystemScheduler scheduler;

    const SystemHandle movement = scheduler.registerSystem(
        SystemDescription{.name = "Movement", .accesses = {write<Transform>()}}, [] {});
    const SystemHandle damage = scheduler.registerSystem(
        SystemDescription{.name = "Damage", .accesses = {write<Health>()}}, [] {});
    const SystemHandle audio = scheduler.registerSystem(
        SystemDescription{.name = "Audio", .accesses = {read<Transform>(), read<Health>()}}, [] {});

    scheduler.rebuildSchedule();

    REQUIRE(scheduler.executionBatches().size() == 2);
    CHECK(scheduler.executionBatches()[0] == std::vector<SystemHandle>{movement, damage});
    CHECK(scheduler.executionBatches()[1] == std::vector<SystemHandle>{audio});
    REQUIRE(scheduler.dependencyEdges().size() == 2);
    CHECK(scheduler.dependencyEdges()[0].after == audio);
    CHECK(scheduler.dependencyEdges()[1].after == audio);
}

TEST_CASE("ECS scheduler treats duplicate read write declarations as writes")
{
    SystemScheduler scheduler;

    const ResourceId transform = resource("Transform");
    const SystemHandle integrator = scheduler.registerSystem(
        SystemDescription{.name = "Integrator", .accesses = {read(transform), write(transform)}},
        [] {});
    const SystemHandle observer = scheduler.registerSystem(
        SystemDescription{.name = "Observer", .accesses = {read(transform)}}, [] {});

    scheduler.rebuildSchedule();

    CHECK(scheduler.executionOrder() == std::vector<SystemHandle>{integrator, observer});
    REQUIRE(scheduler.dependencyEdges().size() == 1);
    CHECK(scheduler.dependencyEdges()[0].beforeAccess == Access::Write);
    CHECK(scheduler.dependencyEdges()[0].afterAccess == Access::Read);
}
