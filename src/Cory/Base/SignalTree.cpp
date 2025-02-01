
#include "SignalTree.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/Math.hpp>

#include <fmt/format.h>

#include <sstream>

namespace Cory {

SignalTree::SignalTree(std::uint64_t signals)
    : maxSignals_{signals}
{
    // signals must be a power of two > 2 cause otherwise why even bother using a signal tree
    if (signals < 2 || (signals & (signals - 1)) != 0) {
        throw std::invalid_argument("SignalTree must be initialized with a power of two signals");
    }

    // We need half as many internal nodes as we have signals
    internalNodes_.resize(signals - 1);

    // We store the signals in the leaf nodes as a bitmask
    const auto leafNodesSize = divideRoundUp(signals, LeafNodeBlock::NUM_BITS);
    leafNodeBlocks_.resize(leafNodesSize);
}

bool SignalTree::set(SignalIdx index)
{
    // Set operations must go bottom-up from the leaf node to the root
    bool before = updateLeafSignal(index, true);
    if (before) {
        // signal was already set
        return false;
    }

    // Update the internal nodes to obtain child-sum property
    for (auto internalNodeIdx = parent(internalNodes_.size() + index); internalNodeIdx != 0;
         internalNodeIdx = parent(internalNodeIdx)) {
        internalNodes_[internalNodeIdx].inc();
    }
    internalNodes_[ROOT_NODE_IDX].inc();
    return true;
}

std::optional<SignalTree::SignalIdx> SignalTree::select()
{
    // To find a signal to clear, we start at the root and go down the tree
    // We decrement the count of the internal nodes as we go
    auto currentNodeIdx = ROOT_NODE_IDX;
    auto updated = internalNodes_[currentNodeIdx].tryDec();

    if (!updated.success) {
        // tree is empty
        return std::nullopt;
    }

    // Note: Once we've decremented the root node, we know that the tree is non-empty
    // and that we *must* find a signal to clear regardless of multithreaded contention
    while (true) {
        auto firstNodeIdx = left(currentNodeIdx);
        auto secondNodeIdx = right(currentNodeIdx);

        // pick left or right child
        // todo bias: std::swap(first_node, second_node);

        if (isNodeInternal(firstNodeIdx)) {
            currentNodeIdx = selectInternalNode(firstNodeIdx, secondNodeIdx);
            continue;
        }

        return selectLeafNode(firstNodeIdx, secondNodeIdx);
    }
}

uint64_t SignalTree::count() const
{
    // By the tree's construction, the count of the root node is equal to the total count
    return internalNodes_[0].count();
}

void SignalTree::validateInternal() const
{
    for (auto i = 0; i < internalNodes_.size(); ++i) {
        if (internalNodes_[i].count() != childSum(i)) {
            throw std::runtime_error{fmt::format("Internal validation failed: Node {} does not "
                                                 "satisfy child sum property! Tree: \n{}",
                                                 i,
                                                 debugPrint())};
        }
    }
}

bool SignalTree::unsafeQueryIsSet(SignalIdx signal) const
{
    auto leafNodeIndex = signal / LeafNodeBlock::NUM_BITS;
    auto leafNodeBit = signal % LeafNodeBlock::NUM_BITS;

    return leafNodeBlocks_[leafNodeIndex].isSet(leafNodeBit);
}

uint64_t SignalTree::childSum(uint64_t index) const
{
    const auto leftNode = left(index);
    const auto rightNode = right(index);

    if (isNodeInternal(leftNode)) {
        // children are internal nodes
        return internalNodes_[leftNode].count() + internalNodes_[rightNode].count();
    }

    // last level has only leaf nodes so we have to query the bitset instead
    const auto leftSignalIndex = leftNode - internalNodes_.size();
    const auto rightSignalIndex = rightNode - internalNodes_.size();
    const auto leftSet = unsafeQueryIsSet(leftSignalIndex) ? 1 : 0;
    const auto rightSet = unsafeQueryIsSet(rightSignalIndex) ? 1 : 0;

    return leftSet + rightSet;
}

SignalTree::NodeIdx SignalTree::selectInternalNode(NodeIdx firstIdx, NodeIdx secondIdx)
{
    if (auto updated = internalNodes_[firstIdx].tryDec(); updated.success) { return firstIdx; }

    auto updated = internalNodes_[secondIdx].tryDec();
    CO_CORE_ASSERT(updated.success, "Internal inconsistency - decrement should always succeed!");
    return secondIdx;
}

SignalTree::NodeIdx SignalTree::selectLeafNode(NodeIdx firstIdx, NodeIdx secondIdx)
{
    // last level has only leaf nodes so we have to query the bitset
    const auto firstSignalIdx = firstIdx - internalNodes_.size();
    const auto secondSignalIdx = secondIdx - internalNodes_.size();

    // If the first leaf signal was set, clear it and return its index
    if (bool firstWasSet = updateLeafSignal(firstSignalIdx, false); firstWasSet) {
        return firstSignalIdx;
    }

    // Otherwise, the second signal must have been set - return it instead
    auto secondWasSet = updateLeafSignal(secondSignalIdx, false);
    CO_CORE_ASSERT(secondWasSet, "Internal inconsistency - second signal should always be set!");
    return secondSignalIdx;
}

bool SignalTree::updateLeafSignal(SignalIdx signal, bool set)
{
    auto leafNodeIndex = signal / LeafNodeBlock::NUM_BITS;
    auto leafNodeBit = signal % LeafNodeBlock::NUM_BITS;

    if (set) { return leafNodeBlocks_[leafNodeIndex].set(leafNodeBit); }

    return leafNodeBlocks_[leafNodeIndex].clear(leafNodeBit);
}

std::string SignalTree::debugPrint() const
{
    std::ostringstream ss;

    ss << "digraph G {\n";

    for (auto i = 0; i < internalNodes_.size(); ++i) {
        ss << fmt::format("{} [label=\"[{}]\"];\n", i, internalNodes_[i].count());
    }
    for (auto i = 0; i < maxSignals_; ++i) {
        const auto leaf_node_idx = i + internalNodes_.size();
        const bool isSet = unsafeQueryIsSet(i);

        ss << fmt::format("{} [label=\"{}: {}\"];\n", leaf_node_idx, i, isSet ? "🔔" : "🔕");
    }

    for (auto i = 0; i < internalNodes_.size(); ++i) {
        auto left = 2 * i + 1;
        auto right = 2 * i + 2;

        ss << fmt::format("{} -> {};\n", i, left);
        ss << fmt::format("{} -> {};\n", i, right);
    }

    ss << "}\n";
    return ss.str();
}

} // namespace Cory