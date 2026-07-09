# Scene Graph

## Load When
- editing entity hierarchy, parent/child metadata, traversal, or entity lifetime
- working on scene data structures before wiring concrete systems
- adding scene consumers in examples or tests

## Main Paths
- `src/Cory/SceneGraph/SceneGraph.hpp` — public entity-tree API: create/remove entities, add/get components, parent/child queries, and traversal generators
- `src/Cory/SceneGraph/SceneGraph.cpp` — root entity setup, recursive removal, depth-first/breadth-first traversal implementation
- `src/Cory/SceneGraph/Common.hpp` — `Entity`, `Component` concept, and forward declarations
- `src/Cory/SceneGraph/System.hpp` — `BasicSystem` CRTP helper, `System` concept, and `CallbackSystem`
- `examples/03-SceneGraph/src/SceneGraphDemo.cpp` — representative consumer

## Important Concepts
- `SceneGraph` wraps an EnTT registry plus `EntityMetaData` for names, parent links, children, and depth
- traversal is exposed as `cppcoro::generator<Entity>` in depth-first and breadth-first order
- `removeEntity()` recursively destroys children and unlinks from the parent
- `sortByDepth<Cmp>()` orders registry views by stored depth

## Read Next
- `src/Cory/SceneGraph/SceneGraph.hpp`
- `src/Cory/SceneGraph/SceneGraph.cpp`
- `src/Cory/Systems/SystemCoordinator.hpp` when wiring systems on top of the graph

## Related Skills
- `cbt` — configure/build/run/test/format Cory code
- `renderer` — when scene data feeds render resources

## Gotchas
- the root entity cannot be destroyed
- recursive removal is simple but can be expensive on large subgraphs
- traversal order depends on insertion order of child lists
