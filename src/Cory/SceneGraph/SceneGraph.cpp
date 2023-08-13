#include "SceneGraph.hpp"

#include <Cory/Base/Log.hpp>

#include <entt/entt.hpp>
#include <vector>

namespace Cory {

struct SceneGraph::SceneGraphPrivate {
    Registry registry;
    Entity root{entt::null};

    void createRootEntity(std::string name)
    {
        CO_CORE_ASSERT(!registry.valid(root), "Root entity already exists");

        root = registry.create();
        registry.emplace<EntityMetaData>(
            root,
            EntityMetaData{
                .name = std::move(name), .parent = entt::null, .children = {}, .depth = 0});
    }
};

SceneGraph::SceneGraph()
    : data_{std::make_unique<SceneGraphPrivate>()}
{
    self().createRootEntity("SceneGraphRoot");
}

SceneGraph::~SceneGraph() {}

Entity SceneGraph::root() const { return self().root; }

const EntityMetaData &SceneGraph::data(Entity entity) const
{
    CO_CORE_ASSERT(valid(entity), "Entity does not exist!");

    return registry().get<EntityMetaData>(entity);
}
Entity SceneGraph::parent(Entity entity) const { return data(entity).parent; }

Entity SceneGraph::createEntity(Entity parent, std::string name)
{
    Entity entity = registry().create();

    uint32_t parentDepth = 0;
    registry().patch<EntityMetaData>(parent, [&parentDepth, entity](EntityMetaData &meta) {
        meta.children.push_back(entity);
        parentDepth = meta.depth;
    });

    registry().emplace<EntityMetaData>(
        entity,
        EntityMetaData{
            .name = std::move(name), .parent = parent, .children = {}, .depth = parentDepth + 1});

    return entity;
}

void SceneGraph::removeEntity(Entity entity)
{
    if (!registry().valid(entity)) { throw EntityException{"Entity does not exist"}; }
    if (entity == self().root) { throw EntityException{"Cannot destroy root entity"}; }

    // first, recursively remove all children
    // note: this is probably super inefficient for larger subgraphs
    // because we keep recursively calling the "patch" on the parent
    // the recursion one level below, but it's probably fine for now
    auto children = data(entity).children;
    for (auto child : children) {
        removeEntity(child);
    }

    // unlink from parent
    auto old_parent = parent(entity);
    registry().patch<EntityMetaData>(
        old_parent, [entity](EntityMetaData &meta) { erase(meta.children, entity); });

    registry().destroy(entity);
}

SceneGraph::Registry &SceneGraph::registry() { return self().registry; }
const SceneGraph::Registry &SceneGraph::registry() const { return self().registry; }
bool SceneGraph::valid(Entity e) const { return registry().valid(e); }

cppcoro::generator<Entity> SceneGraph::depthFirstTraversal() const
{
    std::vector<Entity> stack;
    stack.push_back(root());

    while (!stack.empty()) {
        auto entity = stack.back();
        stack.pop_back();

        co_yield entity;

        auto &meta = data(entity);

        // we want to traverse the children in the order they were inserted,
        // so we reverse the children on the stack - not great for performance
        // but we've probably got bigger fish to fry with this whole design
        stack.insert(stack.end(), meta.children.rbegin(), meta.children.rend());
    }
}

cppcoro::generator<Entity> SceneGraph::breadthFirstTraversal() const
{
    std::deque<Entity> queue;
    queue.push_back(root());

    while (!queue.empty()) {
        auto entity = queue.front();
        queue.pop_front();

        co_yield entity;

        auto &meta = data(entity);
        queue.insert(queue.end(), meta.children.begin(), meta.children.end());
    }
}

cppcoro::generator<Entity> SceneGraph::ancestors(Entity entity) const
{
    while (entity != self().root) {
        entity = parent(entity);
        co_yield entity;
    }
}
} // namespace Cory