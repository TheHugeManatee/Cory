#include "FrameGraph.h"

namespace Cory {

void createTestRenderPass(FrameGraph &frameGraph, FrameGraphResource input,
                          FrameGraphMutableResource output)
{
  struct PassData {
    FrameGraphResource input;
    FrameGraphMutableResource output;
  };

  auto &renderPass = frameGraph.addRenderPass<PassData>(
      "MyRenderPass",
      [&](RenderPassBuilder &builder, PassData &data) {
        data.input = builder.read(input);
        data.output = builder.write(output);
      },
      [=](const PassData &data, const RenderPassResources &resources,
          GraphicsContext *graphicsContext) {
        // issue actual rendering commands through the context
      });
}

} // namespace Cory