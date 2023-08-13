#include <catch2/catch_test_macros.hpp>

#include <Cory/SceneGraph/Common.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <entt/entt.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <array>

using namespace Cory;

struct Transform {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
};
static_assert(Component<Transform>);

struct LinearMotion {
    glm::vec3 velocity;
    glm::vec3 acceleration;
};
static_assert(Component<LinearMotion>);

struct Renderable {
    int mesh;
    int material;
};
static_assert(Component<Renderable>);

struct ComplexThing {
    Transform transform;
    LinearMotion motion;
    Renderable renderable;
};
static_assert(Component<ComplexThing>);

TEST_CASE("Basic Scene Graph API")
{
    SceneGraph sg;

    Entity root = sg.root();
    Entity cube = sg.createEntity(root, "cube");
    auto [transform, motion, renderable] =
        sg.addComponents(root, Transform{}, LinearMotion{}, Renderable{});

    Entity sphere = sg.createEntity(root, "sphere", Transform{}, LinearMotion{}, Renderable{});

    // EntityView view = sg.view<Transform, LinearMotion>();
    // for(auto [e, t, lm] : view){
    //     t.translation += lm.velocity;
    // }
}

TEST_CASE("Adding and removing entities and components")
{
    SceneGraph sg;

    SECTION("Root entity")
    {
        Entity root = sg.root();
        REQUIRE(sg.valid(root));
        auto *meta = sg.getComponent<EntityMetaData>(root);
        REQUIRE(meta != nullptr);
        CHECK(!meta->name.empty());
        CHECK(meta->parent == Entity{entt::null});
        CHECK(meta->children.empty());
        CHECK(meta->depth == 0);
    }
    SECTION("Adding and removing entities")
    {
        Entity root = sg.root();
        Entity entity = sg.createEntity(root, "entity");

        const auto &meta = sg.data(entity);
        CHECK(meta.name == "entity");
        CHECK(meta.parent == root);
        CHECK(meta.children.empty());
        CHECK(meta.depth == 1);

        REQUIRE(sg.data(root).children.size() == 1);
        CHECK(sg.data(root).children[0] == entity);

        sg.removeEntity(entity);
        CHECK(sg.data(root).children.empty());
        CHECK_FALSE(sg.valid(entity));

        CHECK(sg.getComponent<EntityMetaData>(entity) == nullptr);
    }
}

TEST_CASE("Handling hierarchies")
{
    SceneGraph sg;
    SECTION("Building up a hierarchy of entities")
    {
        // depth 0
        Entity root = sg.root();

        // depth 1
        Entity entity = sg.createEntity(root, "entity");
        Entity sibling = sg.createEntity(root, "sibling");
        Entity anotherSibling = sg.createEntity(root, "anotherSibling");

        // depth 2
        Entity child = sg.createEntity(entity, "child");
        Entity siblingChild = sg.createEntity(sibling, "siblingChild");

        // depth 3
        Entity grandchild = sg.createEntity(child, "grandchild");

        REQUIRE(sg.data(root).children.size() == 3);
        CHECK(sg.data(root).children[0] == entity);
        CHECK(sg.data(root).children[1] == sibling);

        REQUIRE(sg.data(entity).children.size() == 1);
        CHECK(sg.data(entity).children[0] == child);

        REQUIRE(sg.data(child).children.size() == 1);
        CHECK(sg.data(child).children[0] == grandchild);

        const auto &meta = sg.data(grandchild);
        CHECK(meta.name == "grandchild");
        CHECK(meta.parent == child);
        CHECK(meta.children.empty());
        CHECK(meta.depth == 3);

        SECTION("The list of ancestors are correct")
        {
            auto rootAncestors = sg.ancestors(root) | ranges::to<std::vector>();
            CHECK(rootAncestors.empty());

            auto entityAncestors = sg.ancestors(entity) | ranges::to<std::vector>();
            CHECK(ranges::equal(std::array{root}, entityAncestors));

            const std::array expectedAncestorsGrandChild{child, entity, root};
            auto grandchildAncestors = sg.ancestors(grandchild) | ranges::to<std::vector>();
            CAPTURE(expectedAncestorsGrandChild);
            CAPTURE(grandchildAncestors);
            CHECK(ranges::equal(expectedAncestorsGrandChild, grandchildAncestors));
        }

        SECTION("Removing a leaf impacts only the parent")
        {
            sg.removeEntity(grandchild);
            CHECK(sg.data(child).children.empty());
            CHECK_FALSE(sg.valid(grandchild));

            CHECK(sg.getComponent<EntityMetaData>(grandchild) == nullptr);
        }
        SECTION("Removing an inner node removes all children")
        {
            sg.removeEntity(entity);

            CHECK(sg.data(root).children.at(0) == sibling);
            CHECK_FALSE(sg.valid(entity));
            CHECK_FALSE(sg.valid(child));
            CHECK_FALSE(sg.valid(grandchild));
        }
        SECTION("Obtaining a Depth-first ordering")
        {
            std::array expectedDepthFirstOrder{
                root, entity, child, grandchild, sibling, siblingChild, anotherSibling};

            auto actualDfsOrder = sg.depthFirstTraversal() | ranges::to<std::vector>();
            CAPTURE(expectedDepthFirstOrder);
            CAPTURE(actualDfsOrder);
            CHECK(ranges::equal(expectedDepthFirstOrder, actualDfsOrder));
        }
        SECTION("Obtaining a Breadth-first ordering")
        {
            std::array expectedBreadthFirstOrder{
                root, entity, sibling, anotherSibling, child, siblingChild, grandchild};
            auto actualBfsOrder = sg.breadthFirstTraversal() | ranges::to<std::vector>();
            CAPTURE(expectedBreadthFirstOrder);
            CAPTURE(actualBfsOrder);
            CHECK(ranges::equal(expectedBreadthFirstOrder, actualBfsOrder));
        }
    }
}

void update_cb(entt::registry &, entt::entity) { fmt::print("Updated!\n"); }
void construct_cb(entt::registry &, entt::entity) { fmt::print("Constructed!\n"); }
void destruct_cb(entt::registry &, entt::entity) { fmt::print("Destructed!\n"); }
struct cmp {
    glm::vec3 pos;
};

TEST_CASE("Entt playground")
{
    entt::registry registry;
    registry.on_construct<cmp>().connect<&construct_cb>();
    registry.on_update<cmp>().connect<&update_cb>();
    registry.on_destroy<cmp>().connect<&destruct_cb>();

    auto entity = registry.create();
    registry.emplace<cmp>(entity, glm::vec3{1, 2, 3});

    registry.patch<cmp>(entity, [](auto &c) {
        fmt::print("Patching..\n");
        c.pos += glm::vec3{1, 1, 1};
    });

    auto view = registry.view<cmp>();

    view.each([](auto entity, cmp &c) {
        fmt::print("Updating through view..\n");
        c.pos += glm::vec3{1, 1, 1};
    });

    view.each([](auto entity, const cmp &c) {
        fmt::print("Printing through const view: {}\n", c.pos.x);
    });
}