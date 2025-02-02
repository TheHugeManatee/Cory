
#include "SignalTree.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/Math.hpp>

#include <fmt/format.h>

#include <sstream>

namespace Cory {

// test/debug assertions
#define CO_SIGNALTREE_ASSERT(cond, msg) CO_CORE_ASSERT(cond, msg)

// Wrapper aroundd atomic for internal node to satisfy putting std::atomic in vector
struct SignalTree::InternalNode {
    std::atomic<uint64_t> count_;

    InternalNode()
        : count_(0)
    {
    }

    InternalNode(const InternalNode &rhs)
        : count_(rhs.count_.load())
    {
    }

    UpdateResult inc() { return {count_.fetch_add(1), true}; }
    UpdateResult tryDec()
    {
        auto expected = count_.load();
        while (expected > 0) {
            auto desired = expected - 1;
            if (count_.compare_exchange_weak(expected, desired)) { return {desired, true}; }
        }
        return {expected, false};
    }
    auto count() const { return count_.load(); }

    InternalNode &operator=(const InternalNode &other)
    {
        count_.store(other.count_.load());
        return *this;
    }
};

// A block of leaf node bits with atomic storage
// each bit represents the signal state of one signal index
struct SignalTree::LeafNodeBlock {
    static constexpr uint64_t NUM_BITS = 64;
    std::atomic<uint64_t> bits_;

    LeafNodeBlock()
        : bits_(0)
    {
    }

    LeafNodeBlock(const LeafNodeBlock &rhs)
        : bits_(rhs.bits_.load())
    {
    }

    LeafNodeBlock &operator=(const LeafNodeBlock &other)
    {
        bits_.store(other.bits_.load());
        return *this;
    }

    // Attempts to set a bit atomically. Returns the previous value of the bit.
    bool set(uint64_t bit)
    {
        const auto mask = 1ull << bit;
        auto previous = bits_.fetch_or(mask);
        return (previous & mask) > 0;
    }

    // Attempts to clear a bit atomically. Returns the previous value of the bit.
    bool clear(uint64_t bit)
    {
        const auto mask = 1ull << bit;
        auto previous = bits_.fetch_and(~mask);
        return (previous & mask) > 0;
    }

    bool isSet(uint64_t bit) const
    {
        auto bits = bits_.load();
        return (bits & (1ull << bit)) != 0;
    }
};

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

SignalTree::~SignalTree() = default;

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

std::optional<SignalTree::SignalIdx> SignalTree::select(uint64_t biasBits)
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

        if (biasBits & 1) { std::swap(firstNodeIdx, secondNodeIdx); }
        biasBits >>= 1;

        if (!isNodeInternal(firstNodeIdx)) { return selectLeafNode(firstNodeIdx, secondNodeIdx); }

        currentNodeIdx = selectInternalNode(firstNodeIdx, secondNodeIdx);
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

bool SignalTree::isNodeInternal(NodeIdx index) const { return index < internalNodes_.size(); }

SignalTree::NodeIdx SignalTree::selectInternalNode(NodeIdx firstIdx, NodeIdx secondIdx)
{
    // Since we have successfully decremented the parent node, we know
    // there *must* be a signal to clear for us in one of the children.
    // However, there might be other consumer and producers threads
    // manipulating the same nodes concurrently. Therefore, we have to
    // keep trying to decrement both of the children until we succeed
    // in decrementing one of them.
    //
    // The sequence of events we're working around would be:
    //     1. Consumer1 decrements firstIdx and fails, then gets suspended
    //     2. Producer inserts a new signal in the first subtree
    //     3. Consumer2 enters this subtree, decrements secondIdx and acquires the signal
    //     4. Consumer1 attempts to decrement secondIdx and also fails :(
    //     5. Consumer1 goes back to firstIdx, which should now succeed.
    //
    // This really only happens mostly near the root of the tree where contention is highest.
    // It is not expected that this loop runs for longer than one or two iterations, but it
    // can theoretically run for longer i very unlucky cases of thread scheduling.
    while (true) {
        if (auto [_, success] = internalNodes_[firstIdx].tryDec(); success) { return firstIdx; }
        if (auto [_, success] = internalNodes_[secondIdx].tryDec(); success) { return secondIdx; }
    }
}

SignalTree::NodeIdx SignalTree::selectLeafNode(NodeIdx firstIdx, NodeIdx secondIdx)
{
    // last level has only leaf nodes so we have to query the bitset
    const auto firstSignalIdx = firstIdx - internalNodes_.size();
    const auto secondSignalIdx = secondIdx - internalNodes_.size();

    // similar to the internal nodes, there may be a multithreaded sequence of
    // events where another thread snatches the secondSignal before we can get
    // to it, while at the same time a producer has set the firstSignal after we
    // have checked it. To handle the case, we need to loop here.
    while (true) {
        if (updateLeafSignal(firstSignalIdx, false)) { return firstSignalIdx; }
        if (updateLeafSignal(secondSignalIdx, false)) { return secondSignalIdx; }
    }
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