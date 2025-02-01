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
 */
class SignalTree : NoCopy, NoMove {
  public:
    using SignalIdx = std::uint64_t;

    /**
     * @brief Create a signal tree with a maximum number of signals.
     * @param signals the number of available individual signals. Must be a power of two.
     */
    explicit SignalTree(std::uint64_t signals);

    /// set a signal by its index
    void set(SignalIdx index);

    /// Query and clear a signal. Returns the index of the signal that was cleared.
    [[nodiscard]] std::optional<SignalIdx> select();

    /// Query the total number of set signals
    [[nodiscard]] uint64_t count() const;

    // Debug/testing methods

    void validateInternal() const;

    /// Query whether a signal is set. this is not threadsafe!
    bool unsafeQueryIsSet(SignalIdx signal) const;

    /// Provide a debug printable representation of the tree
    std::string debugPrint() const;

  private:
    using NodeIdx = std::uint64_t;
    static constexpr NodeIdx ROOT_NODE_IDX = 0;
    struct UpdateResult {
        uint64_t count;
        bool success;
    };
    // Wrapper aroundd atomic for internal node to satisfy putting std::atomic in vector
    struct InternalNode {
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
                if (count_.compare_exchange_strong(expected, desired)) { return {desired, true}; }
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
    struct LeafNodeBlock {
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

        bool set(uint64_t bit)
        {
            auto mask = 1ull << bit;
            auto previous = bits_.fetch_or(mask);
            return (previous & mask) == 0;
        }

        bool clear(uint64_t bit)
        {
            auto mask = 1ull << bit;
            auto previous = bits_.fetch_and(~mask);
            return (previous & mask) != 0;
        }

        bool isSet(uint64_t bit) const
        {
            auto bits = bits_.load();
            return (bits & (1ull << bit)) != 0;
        }
    };

    bool isNodeInternal(NodeIdx index) const { return index < internalNodes_.size(); }
    NodeIdx left(NodeIdx index) const { return 2 * index + 1; }
    NodeIdx right(NodeIdx index) const { return 2 * index + 2; }
    NodeIdx parent(NodeIdx index) const { return (index - 1) / 2; }
    uint64_t childSum(NodeIdx index) const;

    bool updateLeafSignal(SignalIdx signal, bool set);

    uint64_t maxSignals_;
    // Internal nodes store the total number of set signals in their subtree
    std::vector<InternalNode> internalNodes_;
    // leaf nodes store the signal state as a bitset in atomics
    std::vector<LeafNodeBlock> leafNodeBlocks_;
};

} // namespace Cory
