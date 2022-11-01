#pragma once

#include <Cory/Base/Log.hpp>
#include <fmt/core.h>

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace Cory {

struct SlotMapHandle {
    static constexpr uint32_t INVALID_INDEX{0xFFFF'FFFF};
    static constexpr uint32_t FREE_VERSION{0x8000'0000};
    static constexpr uint64_t FREE_BIT{0x8000'0000'0000'0000};

    uint64_t v;

    /// default-constructed handle has invalid version and index
    SlotMapHandle()
        : v{uint64_t{FREE_VERSION} << 32 | INVALID_INDEX}
    {
    }
    /// construct a handle with given index and version
    SlotMapHandle(uint32_t index, uint32_t version = 0)
        : v{uint64_t{version} << 32 | index}
    {
    }
    [[nodiscard]] uint32_t index() const noexcept { return v & 0xFFFF'FFFF; }
    [[nodiscard]] uint32_t version() const noexcept { return v >> 32; }
    [[nodiscard]] bool alive() const noexcept
    {
        return index() != INVALID_INDEX && (v & FREE_BIT) == 0;
    }

    [[nodiscard]] static SlotMapHandle nextVersion(SlotMapHandle old)
    {
#ifdef _DEBUG
        CO_CORE_ASSERT((old.index() & 0x7FFF'FFFF) != 0x7FFF'FFFF, "Version rollover detected!");
#endif
        return {old.index(), old.version() + 1};
    }
    [[nodiscard]] static SlotMapHandle clearFreeBit(SlotMapHandle handle)
    {
        return {handle.index(), handle.version() & 0x7FFF'FFFF};
    }
    [[nodiscard]] static SlotMapHandle setFreeBit(SlotMapHandle handle)
    {
        return {handle.index(), handle.version() & 0x7FFF'FFFF | FREE_VERSION};
    }
    auto operator<=>(const SlotMapHandle &rhs) const = default;
};

/**
 * @brief simple generic slot map for associative storage of objects in contiguous memory
 * roughly following
 * https://web.archive.org/web/20190113012453/https://seanmiddleditch.com/data-structures-for-game-developers-the-slot-map/
 * It probably has some UB in there somewhere when allocating and placement-initializing the
 * chunks but it *seems* to work (on msvc...)
 * TODO:
 *   - Add support for types that chose to intrusively store their identifiers (via concepts?)
 *   - probably more optimal to change memory layout to avoid interleaving data and index
 *   - memory compaction support - implicit or on-demand? how even?
 */
template <typename StoredType_> class SlotMap {
  public:
    using StoredType = StoredType_;
    static constexpr size_t CHUNK_SIZE{64};

    [[nodiscard]] StoredType &operator[](SlotMapHandle id)
    {
        auto &object = validatedGet(id);
        return reinterpret_cast<StoredType &>(object.storage);
    }
    [[nodiscard]] const StoredType &operator[](SlotMapHandle id) const
    {
        return const_cast<SlotMap &>(*this)[id];
    }

    template <typename... InitArgs> SlotMapHandle emplace(InitArgs... args)
    {
        // check if we need to allocate more chunks
        if (freeList_.empty()) {
            // allocate a new chunk and add the leftovers to the free list
            const auto num_chunks = chunkTable_.size();
            Chunk &chunk = *chunkTable_.emplace_back(std::make_unique<Chunk>());

            freeList_.reserve(CHUNK_SIZE);

            for (uint64_t i = CHUNK_SIZE - 1; i > 0; --i) {
                const auto idx = num_chunks * CHUNK_SIZE + i;
                // set version to free marker
                chunk[i].id =
                    SlotMapHandle{static_cast<uint32_t>(idx), SlotMapHandle::FREE_VERSION};
                freeList_.push_back(static_cast<uint32_t>(idx));
            }
            const auto idx = num_chunks * CHUNK_SIZE;
            chunk[0].id = SlotMapHandle{static_cast<uint32_t>(idx), 0};

            new (&chunk[0].storage) StoredType{std::forward<InitArgs>(args)...};
            return chunk[0].id;
        }

        // get the next free index
        int free = freeList_.back();
        freeList_.pop_back();

        auto &object = *objectAt(free);
        CO_CORE_ASSERT(!object.id.alive(), "We got a live object from the free list!");
        object.id = SlotMapHandle::clearFreeBit(object.id);
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
        auto &object = validatedGet(id);
        // increase version and set free bit
        object.id = SlotMapHandle::setFreeBit(SlotMapHandle::nextVersion(id));

        reinterpret_cast<StoredType *>(object.storage)->~StoredType(); // destruct the object
        freeList_.push_back(id.index());
    }

    // update by assigning a new value, will invalidate old handles to the entry
    template <typename ArgumentType> SlotMapHandle update(SlotMapHandle id, ArgumentType &&value)
    {
        auto &object = validatedGet(id);
        reinterpret_cast<StoredType &>(object.storage) = value; // update stored value
        object.id = SlotMapHandle::nextVersion(id);
        return object.id;
    }

    // invalidate the old handle, internally increasing the version of the object
    // use to reflect a semantic change in the value
    SlotMapHandle update(SlotMapHandle id)
    {
        auto &object = validatedGet(id);
        object.id = SlotMapHandle::nextVersion(id); // increase version
        return object.id;
    }

    size_t size() const noexcept { return chunkTable_.size() * CHUNK_SIZE - freeList_.size(); }
    size_t capacity() const noexcept { return chunkTable_.size() * CHUNK_SIZE; }

    bool isValid(SlotMapHandle id) const
    {
        if (!id.alive()) return false;

        auto *object = objectAt(id.index());
        return object != nullptr && object->id == id;
    }

    ~SlotMap()
    {
        if constexpr (!std::is_trivial_v<StoredType>) {
            // call destructor on remaining objects if they are still alive
            for (const auto &chunk : chunkTable_) {
                for (StoredInner_ &object : *chunk) {
                    if (object.id.alive()) {
                        reinterpret_cast<StoredType *>(object.storage)->~StoredType();
                    }
                }
            }
        }
    }

    struct sentinel {};
    struct iterator {
        SlotMap &sm;
        uint32_t index{};

        bool operator==(sentinel) const { return index == sm.size(); }
        iterator &operator++()
        {
            ++index;
            return *this;
        }
        StoredType &operator*()
        {
            auto *object = sm.objectAt(index);
            CO_CORE_ASSERT(object != nullptr, "Object index was invalid!");
            return *reinterpret_cast<StoredType *>(object->storage);
        }
    };

    iterator begin() { return {*this, 0}; }
    sentinel end() { return {}; }

  private:
    struct StoredInner_ {
        SlotMapHandle id;
        std::byte storage[sizeof(StoredType)];
    };
    using Chunk = std::array<StoredInner_, CHUNK_SIZE>;

    StoredInner_ *objectAt(uint32_t index) const
    {
        const uint32_t chunkIndex = index / CHUNK_SIZE;
        const uint32_t elementIndex = index % CHUNK_SIZE;

        if (chunkIndex >= chunkTable_.size()) { return nullptr; }
        return &(*chunkTable_[chunkIndex])[elementIndex];
    }

    StoredInner_ &validatedGet(SlotMapHandle handle)
    {
        if (!handle.alive()) {
            throw std::runtime_error{"Given handle is dead."}; //
        }

        if (handle.index() >= capacity()) {
            throw std::runtime_error{fmt::format(
                "Handle index {} is out of range ({}). Is the handle really from this slot map?!",
                handle.index(),
                size())};
        }
        const uint32_t chunkIndex = handle.index() / CHUNK_SIZE;
        const uint32_t elementIndex = handle.index() % CHUNK_SIZE;

        StoredInner_ &object = (*chunkTable_[chunkIndex])[elementIndex];
        if (object.id != handle) {
            CO_CORE_ASSERT(object.id.index() == handle.index(),
                           "Object index mismatch ({} != {}) - object is in the wrong place!",
                           object.id.index(),
                           handle.index());
            throw std::runtime_error{
                fmt::format("Handle is outdated. Object version = {} but handle version = {}",
                            object.id.version(),
                            handle.version())};
        }
        return object;
    }

  private:
    std::vector<std::unique_ptr<Chunk>> chunkTable_;
    std::vector<uint32_t> freeList_;
};

// convenience handle - less memory efficient but more convenient - stores the slotmap in order
// to resolve the handle when needed
template <typename StoredType> class ResolvableHandle {
  public:
    ResolvableHandle(SlotMap<StoredType> &slotMap, SlotMapHandle handle)
        : slotMap_{slotMap}
        , handle_{handle}
    {
    }

    StoredType &operator*() { return slotMap_[handle_]; }
    const StoredType &operator*() const { return slotMap_[handle_]; }

    StoredType *operator->() { return &slotMap_[handle_]; }
    const StoredType *operator->() const { return &slotMap_[handle_]; }

    SlotMap<StoredType> &slotMap() { return slotMap_; }
    const SlotMap<StoredType> &slotMap() const { return slotMap_; }
    /* implicit */ operator SlotMapHandle() { return handle_; }
    SlotMapHandle handle() const { return handle_; }

  private:
    SlotMap<StoredType> &slotMap_;
    SlotMapHandle handle_;
};

} // namespace Cory

/// make SlotMapHandle formattable
template <> struct fmt::formatter<Cory::SlotMapHandle> {
    template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.end(); }
    auto format(Cory::SlotMapHandle h, format_context &ctx)
    {
        return fmt::format_to(ctx.out(), "{}${}", h.index(), h.version());
    }
};

/// make SlotMapHandle hashable
template <> struct std::hash<Cory::SlotMapHandle> {
    std::size_t operator()(const Cory::SlotMapHandle &s) const noexcept
    {
        return std::hash<uint64_t>{}(s.v);
    }
};