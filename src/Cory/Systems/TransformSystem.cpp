#include <Cory/Systems/TransformSystem.hpp>

#include <Cory/SceneGraph/SceneGraph.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Base/FmtUtils.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace Cory {

namespace {
glm::mat4 parentTransform(SceneGraph &sg, Entity entity)
{
    // find the first ancestor that has a transform
    for (Entity ancestor : sg.ancestors(entity)) {
        auto *transform = sg.getComponent<Transform>(ancestor);
        if (transform) { return transform->worldTransform; }
    }
    return glm::mat4{1.0f};
}
} // namespace

void TransformSystem::beforeUpdate(SceneGraph &sg) { sg.sortByDepth<Transform>(); }

void TransformSystem::update(SceneGraph &sg, TickInfo tickInfo, Entity entity, Transform &transform)
{
    CO_CORE_INFO("Updating transform for {}", sg.data(entity).name);
    glm::mat4 parent{1.0f};

    if (transform.mode == TransformMode::Local) { parent = parentTransform(sg, entity); }

    transform.worldTransform = glm::translate(parent, transform.position) *
                               glm::mat4_cast(transform.rotation) *
                               glm::scale(glm::mat4(1.0f), transform.scale);

    CO_CORE_INFO("Translation for {} is {} (parent={})",
                 sg.data(entity).name,
                 transform.worldTransform[3],
                 parent[3]);
}

void TransformSystem::afterUpdate(SceneGraph &sg) {}
} // namespace Cory
