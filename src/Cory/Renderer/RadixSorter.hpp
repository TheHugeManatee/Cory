#pragma once

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Common.hpp>

namespace Cory {
class RenderTaskBuilder;
class TransientBufferHandle;

class RadixSorter {
  public:
    explicit RadixSorter(Context &ctx);
    ~RadixSorter();

    struct SortOutput {
        TransientBufferHandle keys;
        TransientBufferHandle indices;
    };

    /// Runs the GPU radix sort over the predicate values using framegraph subtasks.
    SortOutput
    sort(RenderTaskBuilder &builder, TransientBufferHandle predicateBuffer, uint32_t instanceCount);

  private:
    Context *ctx_{nullptr};

    ShaderHandle histogramShader_;
    ShaderHandle scanShader_;
    ShaderHandle scatterShader_;
};
} // namespace Cory
