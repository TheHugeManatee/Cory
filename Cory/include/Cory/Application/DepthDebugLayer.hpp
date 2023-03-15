#pragma once

#include <Cory/Application/ApplicationLayer.hpp>

namespace Cory {

class DepthDebugLayer : public ApplicationLayer {
  public:
    DepthDebugLayer()
        : ApplicationLayer("DepthDebug")
    {
    }

    ~DepthDebugLayer() override = default;

    void onAttach(Context &ctx) override;
    void onDetach(Context &ctx) override;
    void onEvent() override;
    void onUpdate() override;

    RenderTaskDeclaration<LayerPassOutputs> renderTask(Cory::RenderTaskBuilder builder,
                                                       LayerPassOutputs previousLayer) override;

  private:
    Cory::ShaderHandle fullscreenTriShader_;
    Cory::ShaderHandle depthDebugShader_;
};

} // namespace Cory