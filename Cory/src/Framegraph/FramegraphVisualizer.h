#pragma once

#include <Cory/Framegraph/Common.hpp>

#include <Cory/Framegraph/Framegraph.hpp>
#include <string>

namespace Cory::Framegraph {

class FramegraphVisualizer {
  public:
    FramegraphVisualizer(const Framegraph &graph)
        : graph_{graph}
    {
    }

    [[nodiscard]] std::string generateDotGraph(const ExecutionInfo &executionInfo) const;

  private:
    const Framegraph &graph_;
};

} // namespace Cory::Framegraph
