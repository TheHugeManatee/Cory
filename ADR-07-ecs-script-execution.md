# ADR-07: ECS Script Execution and Resource-Access Scheduling

## Status
Draft design notes

## Context
Cory already uses a framegraph-style model for renderer work: tasks declare resource access, the engine derives ordering, and runtime execution can be separated from user-authored task order. The same model is a good fit for future gameplay and scripting work.

The goal for the ECS/script layer is to keep gameplay code simple while giving the engine enough information to safely schedule work automatically. Systems and scripts should declare what they read and write; they should not rely on arbitrary source-registration order for correctness.

Primary goals:
1. Keep gameplay and script code natural to author.
2. Allow automatic parallel execution whenever access declarations prove it is safe.
3. Avoid manual scheduling by ECS users.
4. Scale from a single-threaded implementation to many-core systems.
5. Support deterministic execution.
6. Provide a foundation that can later integrate with the renderer framegraph and a general task graph.

## Decision
Model ECS component state, script-visible engine state, and future renderer resources as resources with declared read/write access.

Each ECS system declares the component and engine resources it reads and writes. The engine gathers those declarations, derives dependencies, constructs a directed acyclic execution graph, and executes the graph in a deterministic topological order initially. Later phases can execute independent graph nodes on a thread pool.

Conceptually, ECS scheduling should follow the same producer/consumer model as the render framegraph:

```text
Resource + Read/Write Access + Dependency Analysis + Task Scheduling
```

Rendering resources include textures, buffers, and acceleration structures. Gameplay resources include components, entity state, input state, time, and simulation state.

## ECS Authoring Model
A system should express data access through its function signature or explicit registration metadata.

Example system:

```cpp
struct MoveSystem
{
    void update(Transform &transform, const Velocity &velocity)
    {
        transform.position += velocity.value;
    }
};
```

This implies:
1. `Velocity` is read.
2. `Transform` is written.

An initial explicit registration API can make this contract clear before investing in signature reflection or generated metadata:

```cpp
register_system<MovementSystem>({
    read<Velocity>(),
    write<Transform>(),
});
```

## Access Model
Use a minimal read/write access model first:

```cpp
enum class Access
{
    Read,
    Write,
};

struct ResourceAccess
{
    ResourceId resource;
    Access access;
};

struct SystemDescription
{
    std::vector<ResourceAccess> accesses;
};
```

A system that reads velocity and input while writing transforms would declare:

```text
Movement
    Reads:  Velocity, InputState
    Writes: Transform
```

## Dependency Rules
Dependencies are derived per resource:

| Prior access | Later access | Concurrent? | Dependency required? |
| --- | --- | --- | --- |
| Read | Read | Yes | No |
| Read | Write | No | Yes |
| Write | Read | No | Yes |
| Write | Write | No | Yes |

Read-only sharing scales well. Many systems may read the same component or simulation resource concurrently. Writes are the serialization points.

## Dependency Graph Construction
For every registered system, gather metadata:

```cpp
struct SystemMetadata
{
    ResourceSet reads;
    ResourceSet writes;
};
```

Given:

```text
Movement
    Reads:  Velocity
    Writes: Transform
Animation
    Reads:  Transform
    Writes: Skeleton
Renderer
    Reads:  Transform, Skeleton
```

The dependency graph is:

```text
Movement --> Animation --> Renderer
Movement ------------------> Renderer
```

This graph follows from resource flow:

```text
Velocity  ---> Movement ---> Transform
Transform ---> Animation ---> Skeleton
Skeleton  ------------------> Renderer
Transform ------------------> Renderer
```

The engine should then topologically sort the graph. Independent systems at the same dependency depth can later become parallel work items.

## Parallel Execution Example
Given:

```text
Movement
    Writes: Transform
Damage
    Writes: Health
Audio
    Reads: Transform, Health
```

`Movement` and `Damage` are independent because they touch different resources. `Audio` depends on both because it reads the resources they write.

```text
Movement ----+
             v
            Audio
             ^
Damage ------+
```

A thread-pool scheduler may run `Movement` and `Damage` concurrently, then run `Audio` after both complete.

## Task Graph Generation Pipeline
The intended runtime pipeline is:

```text
Registered Systems
    -> Access Metadata
    -> Dependency Graph
    -> Task Graph
    -> Execution Backend
```

The execution backend can begin as a single-threaded topological traversal and later become a DAG executor backed by `std::execution`, Taskflow, enkiTS, or a Cory-specific scheduler.

## Recommended Implementation Phases

### Phase 1: Single-Threaded Validation
1. Add `ResourceId`, `ResourceAccess`, `SystemDescription`, and system registration metadata.
2. Gather all system read/write declarations.
3. Validate declarations and detect graph cycles.
4. Execute systems single-threaded in derived topological order.
5. Keep deterministic tie-breaking for nodes with no dependency relation.

### Phase 2: DAG Scheduling
1. Build an explicit DAG with system nodes and dependency edges.
2. Persist diagnostics that explain why each edge exists.
3. Topologically sort the DAG per frame or per schedule rebuild.
4. Rebuild only when system/resource metadata changes.

### Phase 3: Parallel Execution
1. Submit ready DAG nodes to a thread pool.
2. Track dependency counters for each node.
3. Enqueue dependents as their incoming counters reach zero.
4. Preserve deterministic behavior through stable graph construction, deterministic tie-breaking, and explicit synchronization points.

### Phase 4: Fine-Grained Resources
Start conservatively with component types as global resources:

```text
Resource = Component Type
```

Then move to finer-grained resources where profitable:

```text
Resource = (Entity, Component)
```

or:

```text
Resource = (Archetype Chunk, Component)
```

The type-level model is conservative because all writes to `Transform` serialize. Entity- or chunk-level resources allow unrelated entities to update concurrently while still tracking cross-entity dependencies.

## Cross-Entity Dependencies
Fine-grained scheduling must support scripts that access other entities.

Example:

```text
EnemyScript
    Reads:  Player.Transform
    Writes: Enemy.Transform
```

If another system writes `Player.Transform`, the enemy script depends on that writer:

```text
Player Update --> EnemyScript
```

This is the same resource dependency rule applied to a more precise resource key.

## Determinism Requirements
The scheduler should be deterministic by construction:

1. Stable system identifiers.
2. Stable resource identifiers.
3. Stable tie-breaking for otherwise independent systems.
4. Explicit schedule rebuild points.
5. Diagnostics for dependency edges and cycles.
6. Clear constraints around side effects such as random number generation, input snapshots, event queues, and external I/O.

Parallel execution should not change observable results compared to the single-threaded topological schedule. If a system performs side effects outside declared resources, it must declare a resource for that side effect or be isolated in a serialized phase.

## Relationship to the Framegraph
The ECS scheduler and renderer framegraph should converge conceptually. Both systems are resource graphs:

| Domain | Resources | Tasks |
| --- | --- | --- |
| Rendering | Textures, buffers, acceleration structures | Render and compute passes |
| Gameplay | Components, entity state, input, time, physics state | ECS systems and scripts |

Long term, both domains can feed a unified task-graph-driven engine architecture. Gameplay systems and render passes become producers and consumers with explicit resource access declarations, allowing the runtime to derive ordering, synchronization, and parallelism.

## Non-Goals for the Initial Pass
1. Full reflection-based system registration.
2. Entity-level or chunk-level dependency precision.
3. Lock-free component storage.
4. A custom high-performance work-stealing scheduler.
5. Integration with renderer pass execution in the first implementation.
6. Hot-reloadable scripting language support.

## Open Questions
1. Should explicit registration remain the long-term API, or should it be generated from system function signatures?
2. Should resources be represented by component type IDs, stable strings, or typed handles?
3. How should command/event buffers declare reads and writes?
4. How should the scheduler expose diagnostics to tests and debug UI?
5. Which external DAG executor, if any, fits Cory's coroutine-heavy architecture best?
6. What is the right boundary between deterministic gameplay scheduling and opportunistic renderer scheduling?

## Implementation Sketch

```cpp
enum class Access
{
    Read,
    Write,
};

struct ResourceAccess
{
    ResourceId resource;
    Access access;
};

struct SystemDescription
{
    std::string_view name;
    std::vector<ResourceAccess> accesses;
};

class SystemScheduler
{
public:
    void registerSystem(SystemDescription description);
    void rebuildSchedule();
    void execute(World &world);
};
```

Initial execution can be deliberately simple:

```text
1. Gather systems.
2. Build dependency graph from read/write metadata.
3. Topologically sort graph.
4. Execute sorted systems single-threaded.
5. Emit diagnostics for dependency edges.
```

This creates the correctness foundation before introducing parallelism.

## Consequences
Positive:
1. Gameplay code remains focused on data transformations.
2. The engine owns ordering and synchronization.
3. The first implementation can be deterministic and single-threaded.
4. The same declarations enable later parallel execution.
5. The model aligns with Cory's existing framegraph direction.

Tradeoffs:
1. Conservative component-type resources may serialize more work than necessary.
2. Users must declare access accurately.
3. Dependency diagnostics become essential for debuggability.
4. Side effects must be modeled explicitly to preserve determinism.
