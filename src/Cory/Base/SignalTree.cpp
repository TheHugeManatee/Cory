
#include "SignalTree.hpp"

#include <Cory/Base/Math.hpp>

Cory::SignalTree::SignalTree(std::uint64_t signals)
{
    // signals must be a power of two > 2
    if (signals < 2 || (signals & (signals - 1)) != 0) {
        throw std::invalid_argument("SignalTree must be initialized with a power of two signals");
    }

    // We need half as many internal nodes as we have signals
    internalNodes_.resize(signals / 2);

    // We store the signals in the leaf nodes as a bitmask
    const auto leafNodesSize = divideRoundUp(signals, LeafNodeBlock::NUM_BITS);
    leafNodes_.resize(leafNodesSize);
}
