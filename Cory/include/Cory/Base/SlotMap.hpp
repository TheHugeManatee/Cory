#pragma once

#include <cstdint> // temp

#include <array>
#include <vector>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

namespace Cory {

using SlotMapHandle = uint64_t;

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

    StoredType &operator[](SlotMapHandle id)
    {
        // throw if handle out of date
        const auto idx = getIndex(id);
        auto &object = objectAt(idx);
        if (object.id != id) { throw std::range_error("Handle is no longer valid!"); }

        return reinterpret_cast<StoredType &>(object.bytes);
    }

    template <typename... InitArgs> SlotMapHandle emplace(InitArgs... args)
    {
        if (freeList_.empty()) {
            // allocate a new chunk and add the leftovers to the free list
            const auto num_chunks = objectChunkTable_.size();
            Chunk &chunk = *objectChunkTable_.emplace_back(std::make_unique<Chunk>());

            freeList_.reserve(CHUNK_SIZE);

            for (uint64_t i = CHUNK_SIZE - 1; i > 0; --i) {
                const auto idx = num_chunks * CHUNK_SIZE + i;
                chunk[i].id = makeIdentifier(idx, 0);
                freeList_.push_back(idx);
            }
            const auto idx = num_chunks * CHUNK_SIZE;
            chunk[0].id = makeIdentifier(static_cast<uint32_t>(idx), 0);

            new (&chunk[0].bytes) StoredType{std::forward<InitArgs>(args)...};
            return chunk[0].id;
        }

        int free = freeList_.back();
        freeList_.pop_back();

        auto& object = objectAt(free);
        new (&object.bytes) StoredType{std::forward<InitArgs>(args)...};
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
        const auto idx = getIndex(id);
        auto &object = objectAt(idx);
        object.id = makeIdentifier(getIndex(id), getVersion(id) + 1);

        reinterpret_cast<StoredType *>(object.bytes)->~StoredType(); // destruct the object
        freeList_.push_back(idx);
    }

    // update by assigning a new value, will invalidate old handles to the entry
    template <typename ArgumentType> SlotMapHandle update(SlotMapHandle id, ArgumentType &&value)
    {
        const auto idx = getIndex(id);
        auto &object = objectAt(idx);
        reinterpret_cast<StoredType &>(object.value) = value;         // update stored value
        object.id = makeIdentifier(getIndex(id), getVersion(id) + 1); // increase version
        return object.id;
    }

    // invalidate the old handle, internally increasing the version of the object
    // use to reflect a semantic change in the value
    SlotMapHandle update(SlotMapHandle id)
    {
        const auto idx = getIndex(id);
        auto &object = objectAt(idx);
        object.id = makeIdentifier(getIndex(id), getVersion(id) + 1); // increase version
        return object.id;
    }

    size_t size() { return objectChunkTable_.size() * CHUNK_SIZE - freeList_.size(); }

  private:
    struct StoredInner_ {
        SlotMapHandle id;
        std::byte bytes[sizeof(StoredType)];
    };
    using Chunk = std::array<StoredInner_, CHUNK_SIZE>;

    static uint32_t getIndex(SlotMapHandle id) { return id & 0xFFFFFFFF; }
    static uint32_t getVersion(SlotMapHandle id) { return id >> 32; }
    static SlotMapHandle makeIdentifier(uint32_t index, uint32_t version)
    {
        return uint64_t{version} << 32 | index;
    }

    StoredInner_ &objectAt(uint32_t index)
    {
        return (*objectChunkTable_[index / CHUNK_SIZE])[index % CHUNK_SIZE];
    }

  private:
    std::vector<std::unique_ptr<Chunk>> objectChunkTable_;
    std::vector<uint32_t> freeList_;
};
} // namespace Cory