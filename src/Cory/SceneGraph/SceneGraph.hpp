#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/SceneGraph/Common.hpp>

#include <cppcoro/generator.hpp>
#include <entt/fwd.hpp>

#include <concepts>
#include <memory>
#include <stdexcept>

namespace Cory {

struct EntityMetaData {
    std::string name;
    Entity parent;
    std::vector<Entity> children;
    uint32_t depth;
};

class EntityException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class SceneGraph : NoCopy {
  public:
    using Registry = entt::basic_registry<Entity>;

    SceneGraph();
    ~SceneGraph();

    // movable
    SceneGraph(SceneGraph &&) noexcept = default;
    SceneGraph &operator=(SceneGraph &&) noexcept = default;

    /// access the root of the scene graph
    Entity root() const;

    /// create an empty entity
    Entity createEntity(Entity parent, std::string name);

    /// create an entity with the given components
    template <Component... Cmps>
    Entity createEntity(Entity parent, std::string name, Cmps &&...components);

    /// check if an entity exists and is valid
    bool valid(Entity e) const;
    /// remove an entity from the scene graph. will destroy its components and unlink it from the
    /// graph
    void removeEntity(Entity entity);

    /// add a single component to an entity
    template <Component Cmp, typename... CmpArgs>
    Cmp &addComponent(Entity entity, CmpArgs &&...args);

    /// add components to an entity
    template <Component... Components>
    std::tuple<Components &...> addComponents(Entity entity, Components &&...components);

    /// access a component. may return nullptr if the entity does not have the component
    template <typename Component> Component *getComponent(Entity entity);
    template <typename Component> const Component *getComponent(Entity entity) const;

    /// access the metadata of a component, like the name, parent or children
    const EntityMetaData &data(Entity entity) const;

    /// get the entities for traversal in depth first order
    cppcoro::generator<Entity> depthFirstTraversal() const;
    cppcoro::generator<Entity> breadthFirstTraversal() const;

    Entity parent(Entity entity) const;
    cppcoro::generator<Entity> ancestors(Entity entity) const;

    template <Component Cmp> void sortByDepth();

  private:
    // since we're not yet implementing the full interface, we'll just expose the registry for now
    template <typename Derived, Component... Cmps> friend class BasicSystem;
    Registry &registry();
    const Registry &registry() const;

    // pimpl'd with convenience accessors for the private data
    class SceneGraphPrivate;
    SceneGraphPrivate &self() { return *data_; }
    const SceneGraphPrivate &self() const { return *data_; }
    std::unique_ptr<SceneGraphPrivate> data_;
};
} // namespace Cory

/** ====================================== Implementation ====================================== **/

#include <entt/entt.hpp>
namespace Cory {

template <Component Cmp, typename... CmpArgs>
Cmp &SceneGraph::addComponent(Entity entity, CmpArgs &&...args)
{
    return registry().emplace<Cmp>(entity, std::forward<CmpArgs>(args)...);
}

template <Component... Cmps>
std::tuple<Cmps &...> SceneGraph::addComponents(Entity entity, Cmps &&...components)
{
    return {(registry().emplace<Cmps>(entity, std::forward<Cmps>(components)))...};
}

template <typename Component> Component *SceneGraph::getComponent(Entity entity)
{
    return registry().try_get<Component>(entity);
}
template <typename Component> const Component *SceneGraph::getComponent(Entity entity) const
{
    return registry().try_get<Component>(entity);
}

template <Component... Cmps>
Entity SceneGraph::createEntity(Entity parent, std::string name, Cmps &&...components)
{
    Entity e = createEntity(parent, name);
    addComponents(e, std::forward<Cmps>(components)...);
    return e;
}

template <Component Cmp> void SceneGraph::sortByDepth()
{
    registry().sort<Cmp>(
        [this](Entity lhs, Entity rhs) { return data(lhs).depth < data(rhs).depth; });
}

} // namespace Cory
