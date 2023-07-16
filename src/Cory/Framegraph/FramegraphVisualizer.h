#pragma once

#include "Cory/Framegraph/Common.hpp"

#include "Cory/Framegraph/Framegraph.hpp"

#include <string>

namespace Cory {

class FramegraphVisualizer {
  public:
    FramegraphVisualizer(const Framegraph &graph)
        : graph_{graph}
    {
    }

    [[nodiscard]] std::string generateDotGraph(const ExecutionInfo &executionInfo) const;

  private:
    friend struct Index;
    void build(Index &index, const ExecutionInfo& executionInfo) const;
    const Framegraph &graph_;
};

} // namespace Cory
