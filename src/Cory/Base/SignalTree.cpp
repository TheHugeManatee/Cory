
#include "SignalTree.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/Math.hpp>

#include <fmt/format.h>

#include <sstream>

namespace Cory {

// test/debug assertions
// #define CO_SIGNALTREE_ASSERT(cond, msg) CO_CORE_ASSERT(cond, msg)
#define CO_SIGNALTREE_ASSERT(cond, msg)

// Wrapper aroundd atomic for internal node to satisfy putting std::atomic in vector
struct SignalTree::InternalNode {
    std::atomic<uint64_t> count_{0};

    InternalNode() = default;
    InternalNode(uint64_t count)
        : count_(count)
    {
    }

    InternalNode(const InternalNode &rhs)
        : count_(rhs.count_.load())
    {
    }

    void inc() { count_.fetch_add(1); }
    bool tryDec()
    {
        auto expected = count_.load();
        while (expected > 0) {
            auto desired = expected - 1;
            if (count_.compare_exchange_weak(expected, desired)) {
                return true;
            }
        }
        return false;
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
    std::atomic<uint64_t> bits_{0};

    LeafNodeBlock() = default;
    LeafNodeBlock(uint64_t bits)
        : bits_(bits)
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

SignalTree::SignalTree(std::uint64_t signals, CreateMode createMode)
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

    if (createMode != CreateMode::FullySignaled) {
        // If we're not creating a fully signaled tree, we're done - leaf
        // nodes and internal nodes are all initialized to zero after this
        leafNodeBlocks_.resize(leafNodesSize);
        return;
    }

    // Set everything to fully signaled
    // start filling lowest level internal nodes, which have a max count of 2
    // then work up level by level, doubling the max count each time
    uint64_t counter_max = 2;
    gsl::index next_node = internalNodes_.size() - 1;

    for (uint64_t nodes_per_level = signals / 2;; nodes_per_level /= 2) {
        for (uint64_t i = 0; i < nodes_per_level; ++i) {
            internalNodes_[next_node].count_ = counter_max;
            --next_node;
        }
        counter_max *= 2;
        if (nodes_per_level == 1) {
            break;
        }
    }
    // fill all leaf node bits to fully set
    leafNodeBlocks_.resize(leafNodesSize, LeafNodeBlock{~0ull});
}

SignalTree::~SignalTree() noexcept = default;

bool SignalTree::set(SignalIdx index) noexcept
{
    CO_CORE_DEBUG_ASSERT(index.has_value(), "SignalTree::set requires a valid signal index");
    CO_CORE_DEBUG_ASSERT(*index < maxSignals_, "SignalTree::set index out of range");
    // Set operations must go bottom-up from the leaf node to the root
    if (bool wasSet = updateLeafSignal(index, true); wasSet) {
        // signal was already set - we don't need to update the tree
        return false;
    }

    // Update the internal nodes to obtain child-sum property
    for (auto internalNodeIdx = parent(internalNodes_.size() + *index);;
         internalNodeIdx = parent(internalNodeIdx)) {
        internalNodes_[internalNodeIdx].inc();
        if (internalNodeIdx == ROOT_NODE_IDX) {
            break;
        }
    }
    return true;
}

SignalTree::SignalIdx SignalTree::select(uint64_t biasBits) noexcept
{
    // To find a signal to clear, we start at the root and go down the tree
    // We decrement the count of the internal nodes as we go
    auto currentNodeIdx = ROOT_NODE_IDX;

    if (!internalNodes_[currentNodeIdx].tryDec()) {
        // tree is empty
        return std::nullopt;
    }

    // Note: Once we've decremented the root node, we know that the tree is non-empty
    // and that we *must* find a signal to clear regardless of multithreaded contention
    while (true) {
        auto firstNodeIdx = left(currentNodeIdx);
        auto secondNodeIdx = right(currentNodeIdx);

        if (biasBits & 1) {
            std::swap(firstNodeIdx, secondNodeIdx);
        }
        biasBits >>= 1;

        if (!isNodeInternal(firstNodeIdx)) {
            return selectLeafNode(firstNodeIdx, secondNodeIdx);
        }

        currentNodeIdx = selectInternalNode(firstNodeIdx, secondNodeIdx);
    }
}

uint64_t SignalTree::count() const noexcept
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
    CO_CORE_DEBUG_ASSERT(signal.has_value(),
                         "SignalTree::unsafeQueryIsSet requires a valid signal index");
    CO_CORE_DEBUG_ASSERT(*signal < maxSignals_,
                         "SignalTree::unsafeQueryIsSet index out of range");
    auto leafNodeBlockIndex = *signal / LeafNodeBlock::NUM_BITS;
    auto leafNodeBit = *signal % LeafNodeBlock::NUM_BITS;

    return leafNodeBlocks_[leafNodeBlockIndex].isSet(leafNodeBit);
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

bool SignalTree::isNodeInternal(NodeIdx index) const noexcept
{
    return index < internalNodes_.size();
}

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
        if (internalNodes_[firstIdx].tryDec()) {
            return firstIdx;
        }
        if (internalNodes_[secondIdx].tryDec()) {
            return secondIdx;
        }
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
        if (updateLeafSignal(firstSignalIdx, false)) {
            return firstSignalIdx;
        }
        if (updateLeafSignal(secondSignalIdx, false)) {
            return secondSignalIdx;
        }
    }
}

bool SignalTree::updateLeafSignal(SignalIdx signal, bool set)
{
    CO_CORE_DEBUG_ASSERT(signal.has_value(),
                         "SignalTree::updateLeafSignal requires a valid signal index");
    CO_CORE_DEBUG_ASSERT(*signal < maxSignals_, "SignalTree::updateLeafSignal index out of range");
    auto leafNodeBlockIndex = *signal / LeafNodeBlock::NUM_BITS;
    auto leafNodeBit = *signal % LeafNodeBlock::NUM_BITS;

    if (set) {
        return leafNodeBlocks_[leafNodeBlockIndex].set(leafNodeBit);
    }

    return leafNodeBlocks_[leafNodeBlockIndex].clear(leafNodeBit);
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
