#pragma once

#include <cstdint> // temp

#include <array>
#include <vector>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

namespace Cory {
// simple generic slot map for associative storage of objects in contiguous memory
// roughly following
// https://web.archive.org/web/20190113012453/https://seanmiddleditch.com/data-structures-for-game-developers-the-slot-map/
// TODO:
//   - Add support for non-trivial types that would need destruction?
//   - Add support for types that chose to intrusively store their identifiers (via concepts?)
//   - probably more optimal to change memory layout to avoid interleaving data and index
//   - add iterator support (needs some additional in-use map?)
//   - memory compaction support - implicit or on-demand?
template <typename StoredType> class SlotMap {
  public:
    static_assert(std::is_trivial_v<StoredType>, "Stored type currently must be trivial");
    static constexpr size_t CHUNK_SIZE{64};
    using Identifier = uint64_t;

    StoredType &operator[](Identifier id)
    {
        // throw if handle out of date
        const auto idx = getIndex(id);
        auto &object = objectAt(idx);
        if (object.id != id) { throw std::range_error("Handle is no longer valid!"); }

        return object.value;
    }

    Identifier acquire()
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
            chunk[0].id = makeIdentifier(idx, 0);
            return chunk[0].id;
        }

        int free = freeList_.back();
        freeList_.pop_back();
        return objectAt(free).id;
    }

    void release(Identifier id)
    {
        const auto idx = getIndex(id);
        auto &object = objectAt(idx);
        object.id = makeIdentifier(getIndex(id), getVersion(id) + 1);
        //object.id = (object.id & 0xFFFFFFFF) | (((object.id >> 32) + 1) << 32);
        freeList_.push_back(idx);
    }

    size_t size() { return objectChunkTable_.size() * CHUNK_SIZE - freeList_.size(); }

  private:
    struct StoredInner_ {
        Identifier id;
        StoredType value;
    };
    using Chunk = std::array<StoredInner_, CHUNK_SIZE>;

    static uint32_t getIndex(Identifier id) { return id & 0xFFFFFFFF; }
    static uint32_t getVersion(Identifier id) { return id >> 32; }
    static Identifier makeIdentifier(uint32_t index, uint32_t version)
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