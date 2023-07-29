#include <Cory/Systems/SystemCoordinator.hpp>

#include <Cory/SceneGraph/System.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace Cory;

struct TestComponent {
    bool systemA{false};
    bool systemB{false};
    bool callbackSystem{false};
};

struct SystemA : public BasicSystem<SystemA, TestComponent> {
    void update(SceneGraph &graph, TickInfo tickInfo, Entity entity, TestComponent &cmp)
    {
        cmp.systemA = true;
    }
};
struct SystemB : public BasicSystem<SystemB, TestComponent> {
    void update(SceneGraph &graph, TickInfo tickInfo, Entity entity, TestComponent &cmp)
    {
        cmp.systemB = true;
    }
};
static_assert(Cory::System<SystemA>);

TEST_CASE("SystemCoordinator ticks all systems")
{
    SceneGraph g;
    auto e = g.createEntity(g.root(), "test", TestComponent{});

    SystemCoordinator coordinator;

    coordinator.emplace<SystemA>();
    coordinator.emplace<SystemB>();
    coordinator.emplace<CallbackSystem<TestComponent>>(
        [&](SceneGraph &, TickInfo, Entity, TestComponent &cmp) {
            cmp.callbackSystem = true;
        });

    TickInfo info{.ticks = 999};

    coordinator.tick(g, info);

    const auto cmp = g.getComponent<TestComponent>(e);
    CHECK(cmp->systemA);
    CHECK(cmp->systemB);
    CHECK(cmp->callbackSystem);
}