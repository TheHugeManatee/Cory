#pragma once

#include <Cory/Base/Common.hpp>

#include <atomic>
#include <cstdint>
#include <vector>

namespace Cory {

/**
 * @brief A signal tree is a threadsafe tree of binary signals.
 *
 * See https://github.com/CppCon/CppCon2024/blob/main/Presentations/Work_Contracts.pdf
 *
 * - Each signal can be either signaled or unsignaled.
 * - The tree is a perfect binary tree
 * - The tree is threadsafe and lockfree
 * - Signals can be set by index, and cleared by requesting the index of an unset signal.
 *
 * The tree is fully threadsafe: Multiple threads can set and select (clear) signals
 * concurrently. There is no guarantee about the order in which signals are selected,
 * however it can be influenced by providing bias bits to the select method.
 *
 * @note: The current implementation is functional, but the representation is not as
 * compact as it could be. To optimally store the tree, the internal nodes should be
 * compacted by storing multiple counters in single atomic variables. Currently,
 * this results in a relatively high storage overhead of 16x higher than optimal memory
 * consumption. The current implementation requires slightly more than 8 bytes per signal.
 *
 */
class SignalTree : NoCopy, NoMove {
  public:
    using SignalIdx = std::uint64_t;

    /**
     * @brief Create a signal tree with a maximum number of signals.
     * @param signals the number of available individual signals. Must be a power of two.
     */
    explicit SignalTree(std::uint64_t signals);

    ~SignalTree() noexcept;

    /// set a signal by its index
    /// Returns true if the signal was set, false if it was already set
    bool set(SignalIdx index) noexcept;

    /// Query and clear a signal. Returns the index of the signal that was cleared.
    [[nodiscard]] std::optional<SignalIdx> select(uint64_t biasBits = 0) noexcept;

    /// Query the current number of set signals.
    [[nodiscard]] uint64_t count() const noexcept;

    // Debug/testing methods
    void validateInternal() const;

    /// Query whether a signal is set. this is not threadsafe, just here for testing/debugging
    [[nodiscard]] bool unsafeQueryIsSet(SignalIdx signal) const;

    /// Provide a debug printable representation of the tree (DOT syntax). This is not threadsafe.
    [[nodiscard]] std::string debugPrint() const;

  private:
    using NodeIdx = std::uint64_t;
    static constexpr NodeIdx ROOT_NODE_IDX = 0;
    struct InternalNode;
    struct LeafNodeBlock;

    bool isNodeInternal(NodeIdx index) const noexcept;
    NodeIdx left(NodeIdx index) const noexcept { return 2 * index + 1; }
    NodeIdx right(NodeIdx index) const noexcept { return 2 * index + 2; }
    NodeIdx parent(NodeIdx index) const noexcept { return (index - 1) / 2; }
    uint64_t childSum(NodeIdx index) const;

    // "Atomically" select one of the two given nodes, decrementing the selected node's count
    NodeIdx selectInternalNode(NodeIdx firstIdx, NodeIdx secondIdx);
    // Atomically select one of the two given nodes, clearing the selected node's signal
    NodeIdx selectLeafNode(NodeIdx firstIdx, NodeIdx secondIdx);
    // Update the signal bit of the signal index. Returns the previous signaling value
    bool updateLeafSignal(SignalIdx signal, bool set);

    uint64_t maxSignals_;
    // Internal nodes store the total number of set signals in their subtree
    std::vector<InternalNode> internalNodes_;
    // leaf nodes store the signal state as a bitset in atomics
    std::vector<LeafNodeBlock> leafNodeBlocks_;
};

} // namespace Cory
