#pragma once

#include "Common.hpp"

#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

#include <glm/vec3.hpp>

class CubeAnimationSystem : public Cory::BasicSystem<CubeAnimationSystem, PointSpriteComponent> {
  public:
    void beforeUpdate(Cory::SceneGraph &sg);

    void update(Cory::SceneGraph &sg,
                Cory::TickInfo tick,
                Cory::Entity entity,
                PointSpriteComponent &anim);

    void drawImguiControls();

  private:
    float numEntities_{0};
};
static_assert(Cory::System<CubeAnimationSystem>);