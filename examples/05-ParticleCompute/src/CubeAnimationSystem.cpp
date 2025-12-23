#include "CubeAnimationSystem.hpp"

#include <Cory/Base/Random.hpp>
#include <Cory/ImGui/Inputs.hpp>

#include <glm/gtx/transform.hpp>

void CubeAnimationSystem::beforeUpdate(Cory::SceneGraph &sg) {}

void CubeAnimationSystem::update(Cory::SceneGraph &sg,
                                 Cory::TickInfo tick,
                                 Cory::Entity entity,
                                 PointSpriteComponent &spriteData)
{
    auto now = gsl::narrow_cast<float>(tick.now.time_since_epoch().count());
}

void CubeAnimationSystem::drawImguiControls()
{
    if (ImGui::Begin("Animation Params")) {

        CoImGui::Input("Number of Entities", numEntities_, 1.0, 10000.0);
    }
    ImGui::End();
}
