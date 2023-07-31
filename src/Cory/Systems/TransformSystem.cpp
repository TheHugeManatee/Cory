#include <Cory/Systems/TransformSystem.hpp>

#include <Cory/SceneGraph/SceneGraph.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace Cory {

using Components::Transform;
using Components::TransformMode;

namespace {
glm::mat4 parentTransform(SceneGraph &sg, Entity entity)
{
    // find the first ancestor that has a transform
    for (Entity ancestor : sg.ancestors(entity)) {
        auto *transform = sg.getComponent<Transform>(ancestor);
        if (transform) { return transform->modelToWorld; }
    }
    return glm::mat4{1.0f};
}
} // namespace

void TransformSystem::beforeUpdate(SceneGraph &sg)
{
    // note - this can potentially be quite expensive
    sg.sortByDepth<Transform>();
}

void TransformSystem::update(SceneGraph &sg, TickInfo tickInfo, Entity entity, Transform &transform)
{
    glm::mat4 parent{1.0f};

    if (transform.mode == TransformMode::Local) { parent = parentTransform(sg, entity); }

    transform.modelToWorld =
        parent * makeTransform(transform.position, transform.rotation, transform.scale);

    //    CO_CORE_INFO("Translation for {} is {} (parent={})",
    //                 sg.data(entity).name,
    //                 transform.worldTransform[3],
    //                 parent[3]);
}

void TransformSystem::afterUpdate(SceneGraph &sg) {}
} // namespace Cory
