#pragma once

#include <Cory/Application/ApplicationLayer.hpp>
#include <Cory/Application/Common.hpp>

#include <kdbindings/property.h>

namespace Cory {

class DepthDebugLayer : public ApplicationLayer {
  public:
    DepthDebugLayer();

    ~DepthDebugLayer() override;

    void onAttach(Context &ctx, LayerAttachInfo info) override;
    void onDetach(Context &ctx) override;
    bool onEvent(Event event) override;
    void onUpdate() override;

    RenderTaskDeclaration<LayerPassOutputs> renderTask(Cory::RenderTaskBuilder builder,
                                                       LayerPassOutputs previousLayer) override;

    kdb::Property<bool> renderEnabled{true};
    kdb::Property<glm::vec2> center{glm::vec2(0.5f, 0.5f)};
    kdb::Property<glm::vec2> size{glm::vec2(0.5f, 0.5f)};
    kdb::Property<glm::vec2> window{glm::vec2(0.0f, 1.0f)};

  private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace Cory