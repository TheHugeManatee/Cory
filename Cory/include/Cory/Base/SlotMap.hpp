#pragma once

#include <fmt/core.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace Cory {

struct SlotMapHandle {
    uint64_t v;
    auto operator<=>(const SlotMapHandle &rhs) const = default;
};

// simple generic slot map for associative storage of objects in contiguous memory
// roughly following
// https://web.archive.org/web/20190113012453/https://seanmiddleditch.com/data-structures-for-game-developers-the-slot-map/
// It likely has some UB in there somewhere when allocating and placement-initializing the
// chunks but it *seems* to work?
// TODO:
//   - Add support for types that chose to intrusively store their identifiers (via concepts?)
//   - probably more optimal to change memory layout to avoid interleaving data and index
//   - add iterator support (needs some additional in-use map?)
//   - memory compaction support - implicit or on-demand? how even?
template <typename StoredType> class SlotMap {
  public:
    static constexpr size_t CHUNK_SIZE{64};
    static constexpr uint32_t FREE_VERSION{0xFFFFFFFF};

    StoredType &operator[](SlotMapHandle id)
    {
        assert(version(id) != FREE_VERSION);
        // throw if handle out of date
        const auto idx = index(id);
        auto &object = objectAt(idx);
        if (object.id != id) { throw std::range_error("Handle is no longer valid!"); }

        return reinterpret_cast<StoredType &>(object.storage);
    }

    template <typename... InitArgs> SlotMapHandle emplace(InitArgs... args)
    {
        // check if we need to allocate more chunks
        if (freeList_.empty()) {
            // allocate a new chunk and add the leftovers to the free list
            const auto num_chunks = objectChunkTable_.size();
            Chunk &chunk = *objectChunkTable_.emplace_back(std::make_unique<Chunk>());

            freeList_.reserve(CHUNK_SIZE);

            for (uint64_t i = CHUNK_SIZE - 1; i > 0; --i) {
                const auto idx = num_chunks * CHUNK_SIZE + i;
                // set version to free marker
                chunk[i].id = makeHandle(static_cast<uint32_t>(idx), FREE_VERSION);
                freeList_.push_back(static_cast<uint32_t>(idx));
            }
            const auto idx = num_chunks * CHUNK_SIZE;
            chunk[0].id = makeHandle(static_cast<uint32_t>(idx), 0);

            new (&chunk[0].storage) StoredType{std::forward<InitArgs>(args)...};
            return chunk[0].id;
        }

        // get the next free index
        int free = freeList_.back();
        freeList_.pop_back();

        auto &object = objectAt(free);
        // reset version to zero
        object.id = makeHandle(index(object.id), 0);
        // construct the object in the storage
        new (&object.storage) StoredType{std::forward<InitArgs>(args)...};
        return object.id;
    }

    /// insert a default-constructed element
    SlotMapHandle insert() { return emplace(); }
    /// insert an rvalue (move into the slotmap)
    SlotMapHandle insert(StoredType &&value) { return emplace(std::move(value)); }
    /// insert a copy of an element
    SlotMapHandle insert(const StoredType &value) { return emplace(value); }

    // release the object, invalidating previous handles and reclaiming the memory for future use
    void release(SlotMapHandle id)
    {
        const auto idx = index(id);
        auto &object = objectAt(idx);
        object.id = makeHandle(index(id), FREE_VERSION); // reset version to free marker

        reinterpret_cast<StoredType *>(object.storage)->~StoredType(); // destruct the object
        freeList_.push_back(idx);
    }

    // update by assigning a new value, will invalidate old handles to the entry
    template <typename ArgumentType> SlotMapHandle update(SlotMapHandle id, ArgumentType &&value)
    {
        assert(version(id) != FREE_VERSION);
        const auto idx = index(id);
        auto &object = objectAt(idx);
        if (object.id != id) { throw std::range_error("Handle is no longer valid!"); }
        reinterpret_cast<StoredType &>(object.storage) = value; // update stored value
        object.id = makeHandle(index(id), version(id) + 1);     // increase version
        return object.id;
    }

    // invalidate the old handle, internally increasing the version of the object
    // use to reflect a semantic change in the value
    SlotMapHandle update(SlotMapHandle id)
    {
        assert(version(id) != FREE_VERSION);
        const auto idx = index(id);
        auto &object = objectAt(idx);
        if (object.id != id) { throw std::range_error("Handle is no longer valid!"); }
        object.id = makeHandle(index(id), version(id) + 1); // increase version
        return object.id;
    }

    size_t size() { return objectChunkTable_.size() * CHUNK_SIZE - freeList_.size(); }

    ~SlotMap()
    {
        if constexpr (!std::is_trivial_v<StoredType>) {
            // call destructor on remaining objects if they are still alive
            for (const auto &chunk : objectChunkTable_) {
                for (StoredInner_ &object : *chunk) {
                    if (version(object.id) != FREE_VERSION) {
                        reinterpret_cast<StoredType *>(object.storage)->~StoredType();
                    }
                }
            }
        }
    }

  private:
    struct StoredInner_ {
        SlotMapHandle id;
        std::byte storage[sizeof(StoredType)];
    };
    using Chunk = std::array<StoredInner_, CHUNK_SIZE>;

    static uint32_t index(SlotMapHandle id) { return id.v & 0xFFFFFFFF; }
    static uint32_t version(SlotMapHandle id) { return id.v >> 32; }
    static SlotMapHandle makeHandle(uint32_t index, uint32_t version)
    {
        return {uint64_t{version} << 32 | index};
    }

    StoredInner_ &objectAt(uint32_t index)
    {
        const uint32_t chunkIndex = index / CHUNK_SIZE;
        const uint32_t elementIndex = index % CHUNK_SIZE;

        if (chunkIndex >= objectChunkTable_.size()) {
            throw std::range_error("Handle is out of range!");
        }
        return (*objectChunkTable_[chunkIndex])[elementIndex];
    }

  private:
    std::vector<std::unique_ptr<Chunk>> objectChunkTable_;
    std::vector<uint32_t> freeList_;
};
} // namespace Cory

/// make SlotMapHandle formattable
template <> struct fmt::formatter<Cory::SlotMapHandle> {
    template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.end(); }
    auto format(Cory::SlotMapHandle h, format_context &ctx)
    {
        return fmt::format_to(ctx.out(), "{}${}", h.v & 0xFFFFFFFF, h.v >> 32);
    }
};

/// make SlotMapHandle hashable
template <> struct std::hash<Cory::SlotMapHandle> {
    std::size_t operator()(const Cory::SlotMapHandle &s) const noexcept
    {
        return std::hash<uint64_t>{}(s.v);
    }
};