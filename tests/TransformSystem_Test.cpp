#include <Cory/SceneGraph/SceneGraph.hpp>

#include <catch2/catch_test_macros.hpp>

#include <Cory/Systems/TransformSystem.hpp>

using namespace Cory;
using Components::Transform;

TEST_CASE("CallbackSystem")
{
    SceneGraph sg;

    CallbackSystem<Transform> updater{
        [](SceneGraph &graph, TickInfo tickInfo, Entity entity, Transform &transform) {
            transform.position += glm::vec3(1.0f);
        }};
    static_assert(System<CallbackSystem<Transform>>); // make sure we implement the system interface

    Entity entity = sg.createEntity(sg.root(), "entity", Transform{.position{1.0f, 1.0f, 1.0f}});
    updater.tick(sg, {});

    CHECK(sg.getComponent<Transform>(entity)->position == glm::vec3(2.0f, 2.0f, 2.0f));
    updater.tick(sg, {});
    CHECK(sg.getComponent<Transform>(entity)->position == glm::vec3(3.0f, 3.0f, 3.0f));
}

TEST_CASE("Transform System works")
{

    SceneGraph sg;

    Entity root = sg.root();
    Entity entity = sg.createEntity(root, "entity");
    Transform &entityTransform = sg.addComponent<Transform>(entity);

    Entity child = sg.createEntity(entity, "child");
    Transform &childTransform =
        sg.addComponent<Transform>(child, Transform{.position{1.0f, 1.0f, 0.0f}});

    Entity grandchild = sg.createEntity(child, "grandchild");
    Transform &grandchildTransform = sg.addComponent<Transform>(grandchild);

    TransformSystem ts;
    ts.tick(sg, {});

    CHECK(sg.getComponent<Transform>(entity)->modelToWorld[3] ==
          glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(sg.getComponent<Transform>(child)->modelToWorld[3] ==
          glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    CHECK(sg.getComponent<Transform>(grandchild)->modelToWorld[3] ==
          glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

    sg.getComponent<Transform>(entity)->scale = glm::vec3(2.0f, 1.0f, 1.0f);
    sg.getComponent<Transform>(grandchild)->position = glm::vec3(-1.0f, 0.0f, 0.0f);
    ts.tick(sg, {});

    CHECK(sg.getComponent<Transform>(entity)->modelToWorld[3] ==
          glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(sg.getComponent<Transform>(child)->modelToWorld[3] ==
          glm::vec4(2.0f, 1.0f, 0.0f, 1.0f));
    CHECK(sg.getComponent<Transform>(grandchild)->modelToWorld[3] ==
          glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    // todo check rotation at some point
}