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

class ImGuiLayer {
  public:
    ImGuiLayer();
    ~ImGuiLayer();

    void init(Window &window, Context &ctx, VkImageView renderedImage);

    void deinit(Context &ctx);

    void newFrame(Context &ctx);
    void drawFrame(Context &ctx, uint32_t currentFrameIdx);

  private:
    struct Private;
    std::unique_ptr<Private> data_;
};

} // namespace Cory
