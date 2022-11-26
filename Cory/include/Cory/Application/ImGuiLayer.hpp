/*
 * Copyright 2022 OneProjects Design Innovation Limited
 * Company Number 606427, Ireland
 * All rights reserved
 */

#pragma once

#include <cstdint>
#include <memory>

typedef struct VkImageView_T *VkImageView;

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
    void recordFrameCommands(Context &ctx, FrameContext& frameCtx);

  private:
    struct Private;
    std::unique_ptr<Private> data_;
    void setupCustomColors();
};

} // namespace Cory
