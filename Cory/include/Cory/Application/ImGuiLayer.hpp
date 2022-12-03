/*
 * Copyright 2022 OneProjects Design Innovation Limited
 * Company Number 606427, Ireland
 * All rights reserved
 */

#pragma once

#include <Magnum/Vk/Vk.h>

#include <cstdint>
#include <memory>

using VkImageView = struct VkImageView_T *;

namespace Cory {

class Context;
class Window;
class FrameContext;

class ImGuiLayer {
  public:
    ImGuiLayer();
    ~ImGuiLayer();

    void init(Window &window, Context &ctx);
    void deinit(Context &ctx);

    void newFrame(Context &ctx);
    void recordFrameCommands(Context &ctx, uint32_t frameIdx, Magnum::Vk::CommandBuffer &cmdBuffer);

  private:
    struct Private;
    std::unique_ptr<Private> data_;
    void setupCustomColors();
};

} // namespace Cory
