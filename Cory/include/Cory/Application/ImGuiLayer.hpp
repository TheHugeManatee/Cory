/*
 * Copyright 2022 OneProjects Design Innovation Limited
 * Company Number 606427, Ireland
 * All rights reserved
 */

#pragma once

#include <Magnum/Vk/Vk.h>

#include <Cory/Application/ApplicationLayer.hpp>

#include <cstdint>
#include <memory>

using VkImageView = struct VkImageView_T *;

namespace Cory {

class Context;
class Window;
class FrameContext;

class ImGuiLayer : public ApplicationLayer {
  public:
    ImGuiLayer(Window& window);
    ~ImGuiLayer();

    void onAttach(Context &ctx, LayerAttachInfo info) override;
    void onDetach(Context &ctx) override;
    bool onEvent(Event event) override;
    void onUpdate() override;
    bool hasRenderTask() const override { return true; }
    RenderTaskDeclaration<LayerPassOutputs> renderTask(Cory::RenderTaskBuilder builder,
                                                       LayerPassOutputs previousLayer) override;
  private:
    void newFrame(Context &ctx);
    void recordFrameCommands(Context &ctx, uint32_t frameIdx, Magnum::Vk::CommandBuffer &cmdBuffer);

    struct Private;
    std::unique_ptr<Private> data_;
    void setupCustomColors();
};

} // namespace Cory
