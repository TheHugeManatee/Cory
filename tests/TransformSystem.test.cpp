#include <Cory/SceneGraph/SceneGraph.hpp>

#include <catch2/catch_test_macros.hpp>

#include <Cory/Systems/TransformSystem.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>

using namespace Cory;

template <typename... Components>
class CallbackSystem : public SimpleSystem<CallbackSystem<Components...>, Components...> {
  public:
    template <typename Fn>
    CallbackSystem(Fn &&fn)
        : updateFn_(std::forward<Fn>(fn))
    {
    }

    void beforeUpdate(SceneGraph &graph){};
    void update(SceneGraph &graph, TickInfo tickInfo, Entity entity, Components &...components)
    {
        updateFn_(graph, tickInfo, entity, components...);
    }
    void afterUpdate(SceneGraph &graph){};

  private:
    std::function<void(SceneGraph &, TickInfo, Entity, Components &...)> updateFn_;
};

TEST_CASE("CallbackSystem")
{
    SceneGraph sg;

    CallbackSystem<Transform> updater{
        [](SceneGraph &graph, TickInfo tickInfo, Entity entity, Transform &transform) {
            transform.position += glm::vec3(1.0f);
        }};
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
    ts.tick(sg, TickInfo{});

    CHECK(sg.getComponent<Transform>(entity)->worldTransform[3] ==
          glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(sg.getComponent<Transform>(child)->worldTransform[3] ==
          glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    CHECK(sg.getComponent<Transform>(grandchild)->worldTransform[3] ==
          glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

    sg.getComponent<Transform>(entity)->scale = glm::vec3(2.0f, 1.0f, 1.0f);
    sg.getComponent<Transform>(grandchild)->position = glm::vec3(-1.0f, 0.0f, 0.0f);
    ts.tick(sg, TickInfo{});

    CHECK(sg.getComponent<Transform>(entity)->worldTransform[3] ==
          glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(sg.getComponent<Transform>(child)->worldTransform[3] ==
          glm::vec4(2.0f, 1.0f, 0.0f, 1.0f));
    CHECK(sg.getComponent<Transform>(grandchild)->worldTransform[3] ==
          glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
}