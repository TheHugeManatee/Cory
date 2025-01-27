#pragma once

#include <Cory/Base/Common.hpp>

#include <atomic>
#include <cstdint>
#include <vector>

namespace Cory {

/**
 * @brief A signal tree is a threadsafe tree of binary signals.
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
    [[nodiscard]] std::optional<SignalIdx> clearNext();

    /// Query the total number of set signals
    [[nodiscard]] uint64_t count() const;

    // Debug/testing methods

    void validateInternal() const;

    /// Query whether a signal is set. this is not threadsafe!
    bool unsafeQueryIsSet(SignalIdx signal) const;

    /// Provide a debug printable representation of the tree
    std::string debugPrint() const;

  private:
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

        auto inc() { return count_.fetch_add(1); }
        auto dec() { return count_.fetch_sub(1); }
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

    auto left(uint64_t index) const { return 2 * index + 1; }
    auto right(uint64_t index) const { return 2 * index + 2; }
    auto parent(uint64_t index) const { return (index - 1) / 2; }
    uint64_t childSum(uint64_t index) const;

    bool updateLeaf(SignalIdx signal, bool set);

    uint64_t maxSignals_;
    // Internal nodes store the total number of set signals in their subtree
    std::vector<InternalNode> internalNodes_;
    // leaf nodes store the signal state as a bitset in atomics
    std::vector<LeafNodeBlock> leafNodeBlocks_;
};

} // namespace Cory
