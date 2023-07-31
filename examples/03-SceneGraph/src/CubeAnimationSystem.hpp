#pragma once

#include "Common.hpp"

#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

#include <glm/vec3.hpp>

struct AnimationData {
    int num_cubes{20000};
    float blend{0.8f};

    struct param {
        float val;
        float min;
        float max;
        operator float() const { return val; }
    };
    param ti{1.5f, 0.0f, 10.0f};
    param tsi{2.0f, 0.0f, 3.0f};
    param tsf{100.0f, 0.0f, 250.0f};
    param r0{0.0f, -2.0f, 2.0f};
    param rt{-0.1f, -2.0f, 2.0f};
    param ri{1.3f, -2.0f, 2.0f};
    param rti{0.05f, -2.0f, 2.0f};
    param s0{0.05f, 0.0f, 1.0f};
    param st{0.0f, -0.01f, 0.01f};
    param si{0.4f, 0.0f, 2.0f};
    param c0{-0.75f, -2.0f, 2.0f};
    param cf0{2.0f, -10.0f, 10.0f};
    param cfi{-0.5f, -2.0f, 2.0f};

    glm::vec3 translation{0.0, 0.0f, 2.5f};
    glm::vec3 rotation{0.0f};
};

class CubeAnimationSystem : public Cory::BasicSystem<CubeAnimationSystem,
                                                     AnimationComponent,
                                                     Cory::Components::Transform> {
  public:
    void beforeUpdate(Cory::SceneGraph &sg);

    void update(Cory::SceneGraph &sg,
                Cory::TickInfo tick,
                Cory::Entity entity,
                AnimationComponent &anim,
                Cory::Components::Transform &transform);

    void drawImguiControls();

  private:
    void animate(AnimationComponent &d, Cory::Components::Transform &transform, float t);
    void randomize(AnimationData::param &p);
    void randomize(AnimationData &ad);

    float numEntities_{0};
    AnimationData ad_;
};
static_assert(Cory::System<CubeAnimationSystem>);