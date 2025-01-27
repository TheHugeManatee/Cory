
#include "SignalTree.hpp"

#include <Cory/Base/Math.hpp>

#include <fmt/format.h>

#include <sstream>

namespace Cory {

SignalTree::SignalTree(std::uint64_t signals)
    : maxSignals_{signals}
{
    // signals must be a power of two > 2
    if (signals < 2 || (signals & (signals - 1)) != 0) {
        throw std::invalid_argument("SignalTree must be initialized with a power of two signals");
    }

    // We need half as many internal nodes as we have signals
    internalNodes_.resize(signals - 1);

    // We store the signals in the leaf nodes as a bitmask
    const auto leafNodesSize = divideRoundUp(signals, LeafNodeBlock::NUM_BITS);
    leafNodeBlocks_.resize(leafNodesSize);
}

void SignalTree::set(SignalIdx index)
{
    // Set operations must go bottom-up from the leaf node to the root
    updateLeaf(index, true);

    // Update the internal nodes to obtain child-sum property
    for (auto internal_node_idx = parent(internalNodes_.size() + index); internal_node_idx != 0;
         internal_node_idx = parent(internal_node_idx)) {
        internalNodes_[internal_node_idx].inc();
    }
    internalNodes_[0].inc();
}

std::optional<SignalTree::SignalIdx> SignalTree::clearNext()
{

    // To find a signal to clear, we start at the root and go down the tree
    // We decrement the count of the internal nodes as we go
    auto current_node = 0;
    auto prev = internalNodes_[current_node].dec();

    // tree is empty
    if (prev == 0) {
        // we just underflowed the root node, so re-add 1 to it
        internalNodes_[current_node].inc();
        return std::nullopt;
    }

    while (current_node < internalNodes_.size()) {
        auto first_node = left(current_node);
        auto second_node = right(current_node);

        // pick left or right child
        // todo bias: std::swap(first_node, second_node);

        if (first_node < internalNodes_.size()) {
            if (prev = internalNodes_[first_node].dec(); prev > 0) { current_node = first_node; }
            else {
                internalNodes_[first_node].inc();
                // it was actually zero so we decremented it "below zero" - re-increment it
                if (prev = internalNodes_[second_node].dec(); prev > 0) {
                    current_node = second_node;
                }
                else {
                    // another thread has snatched the signal from us?!
                    internalNodes_[second_node].inc();
                    return std::nullopt;
                }
            }
        }
        else {
            // last level has only leaf nodes so we have to query the bitset instead
            const auto first_signal_index = first_node - internalNodes_.size();
            const auto second_signal_index = second_node - internalNodes_.size();

            // If the first leaf signal was set, clear it and return its index
            if (updateLeaf(first_signal_index, false)) { return first_signal_index; }

            // Otherwise, the second signal must have been set - return it instead
            updateLeaf(second_signal_index, false);
            return second_signal_index;
        }
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
    const auto left_node = left(index);
    const auto right_node = right(index);

    if (left_node < internalNodes_.size()) {
        // children are internal nodes
        return internalNodes_[left_node].count() + internalNodes_[right_node].count();
    }

    // last level has only leaf nodes so we have to query the bitset instead
    const auto left_signal_index = left_node - internalNodes_.size();
    const auto right_signal_index = right_node - internalNodes_.size();
    const auto left_set = unsafeQueryIsSet(left_signal_index) ? 1 : 0;
    const auto right_set = unsafeQueryIsSet(right_signal_index) ? 1 : 0;

    return left_set + right_set;
}

bool SignalTree::updateLeaf(SignalIdx signal, bool set)
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