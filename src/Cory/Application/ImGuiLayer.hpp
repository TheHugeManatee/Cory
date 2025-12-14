/*
 * Copyright 2022 OneProjects Design Innovation Limited
 * Company Number 606427, Ireland
 * All rights reserved
 */

#pragma once

#include <Cory/Application/ApplicationLayer.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <cstdint>
#include <memory>

namespace Cory {

class Context;
class Window;
class FrameContext;

class ImGuiLayer : public ApplicationLayer {
  public:
    ImGuiLayer(Window &window);
    ~ImGuiLayer() override;

    void onAttach(Context &ctx, LayerAttachInfo info) override;
    void onDetach(Context &ctx) override;
    bool onEvent(Event event) override;
    void onUpdate(const LogicUpdateContext &updateCtx) override;
    bool hasRenderTask() const override { return true; }
    RenderTaskDeclaration<LayerPassOutputs> renderTask(RenderTaskBuilder builder,
                                                       LayerPassOutputs previousLayer) override;

    // this is mostly still public so we can use the layer in an
    // application that does not use a frame graph
    void recordFrameCommands(FrameContext &frameCtx, Gpu::RenderPassCommandRecorder *recorder);

  private:
    struct Private;
    std::unique_ptr<Private> data_;
    void setupCustomColors();
};

} // namespace Cory
