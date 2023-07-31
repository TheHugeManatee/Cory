#include "CubeAnimationSystem.hpp"

#include <Cory/Base/Random.hpp>
#include <Cory/ImGui/Inputs.hpp>

#include <glm/gtx/transform.hpp>

void CubeAnimationSystem::beforeUpdate(Cory::SceneGraph &sg)
{
    if (numEntities_ != ad_.num_cubes) {
        // todo what if we reduce num_cubes?
        while (numEntities_ < ad_.num_cubes) {
            sg.createEntity(sg.root(),
                            fmt::format("cube_{}", numEntities_),
                            AnimationComponent{},
                            Cory::Components::Transform{});
            numEntities_ += 1.0f;
        }
        forEach<AnimationComponent>(sg,
                                    [total = numEntities_, entityIndex = 0.0f](
                                        Cory::Entity e, AnimationComponent &anim) mutable {
                                        anim.entityIndex = entityIndex / total;
                                        entityIndex += 1.0f;
                                    });
    }
}

void CubeAnimationSystem::update(Cory::SceneGraph &sg,
                                 Cory::TickInfo tick,
                                 Cory::Entity entity,
                                 AnimationComponent &anim,
                                 Cory::Components::Transform &transform)
{
    auto now = gsl::narrow_cast<float>(tick.now.time_since_epoch().count());
    animate(anim, transform, now);
}

void CubeAnimationSystem::drawImguiControls()
{
    if (ImGui::Begin("Animation Params")) {

        if (ImGui::Button("Randomize")) { randomize(ad_); }

        CoImGui::Input("Cubes", ad_.num_cubes, 1, 10000);
        CoImGui::Slider("blend", ad_.blend, 0.0f, 1.0f);
        CoImGui::Slider("translation", ad_.translation, -3.0f, 3.0f);
        CoImGui::Slider("rotation", ad_.rotation, -glm::pi<float>(), glm::pi<float>());

        CoImGui::Slider("ti", ad_.ti.val, ad_.ti.min, ad_.ti.max);
        CoImGui::Slider("tsi", ad_.tsi.val, ad_.tsi.min, ad_.tsi.max);
        CoImGui::Slider("tsf", ad_.tsf.val, ad_.tsf.min, ad_.tsf.max);
        CoImGui::Slider("r0", ad_.r0.val, ad_.r0.min, ad_.r0.max);
        CoImGui::Slider("rt", ad_.rt.val, ad_.rt.min, ad_.rt.max);
        CoImGui::Slider("ri", ad_.ri.val, ad_.ri.min, ad_.ri.max);
        CoImGui::Slider("rti", ad_.rti.val, ad_.rti.min, ad_.rti.max);
        CoImGui::Slider("s0", ad_.s0.val, ad_.s0.min, ad_.s0.max);
        CoImGui::Slider("st", ad_.st.val, ad_.st.min, ad_.st.max);
        CoImGui::Slider("si", ad_.si.val, ad_.si.min, ad_.si.max);
        CoImGui::Slider("c0", ad_.c0.val, ad_.c0.min, ad_.c0.max);
        CoImGui::Slider("cf0", ad_.cf0.val, ad_.cf0.min, ad_.cf0.max);
        CoImGui::Slider("cfi", ad_.cfi.val, ad_.cfi.min, ad_.cfi.max);
    }
    ImGui::End();
}
void CubeAnimationSystem::animate(AnimationComponent &d,
                                  Cory::Components::Transform &transform,
                                  float t)
{
    float i = d.entityIndex;
    const float angle = ad_.r0 + ad_.rt * t + ad_.ri * i + ad_.rti * i * t;
    const float scale = ad_.s0 + ad_.st * t + ad_.si * i;

    const float tsf = ad_.tsf / 2.0f + ad_.tsf * sin(t / 10.0f);
    const glm::vec3 translation{sin(i * tsf) * i * ad_.tsi, cos(i * tsf) * i * ad_.tsi, i * ad_.ti};

    transform.position = ad_.translation + translation;
    transform.rotation = ad_.rotation + glm::vec3{0.0f, angle, angle / 2.0f};
    transform.scale = glm::vec3{scale};

    const float colorFreq = 1.0f / (ad_.cf0 + ad_.cfi * i);
    const float brightness = i + 0.2f * abs(sin(t + i));
    const float r = ad_.c0 * t * colorFreq;
    const glm::vec4 start{0.8f, 0.2f, 0.2f, 1.0f};
    const glm::mat4 cm = glm::rotate(
        glm::scale(glm::mat4{1.0f}, glm::vec3{brightness}), r, glm::vec3{1.0f, 1.0f, 1.0f});

    d.color = start * cm;
    d.blend = ad_.blend;
}

void CubeAnimationSystem::randomize(AnimationData::param &p)
{
    p.val = Cory::RNG::Uniform(p.min, p.max);
}

void CubeAnimationSystem::randomize(AnimationData &ad)
{
    randomize(ad_.ti);
    randomize(ad_.tsi);
    randomize(ad_.tsf);
    randomize(ad_.r0);
    randomize(ad_.rt);
    randomize(ad_.ri);
    randomize(ad_.rti);
    randomize(ad_.s0);
    randomize(ad_.st);
    randomize(ad_.si);
    randomize(ad_.c0);
    randomize(ad_.cf0);
    randomize(ad_.cfi);
}
