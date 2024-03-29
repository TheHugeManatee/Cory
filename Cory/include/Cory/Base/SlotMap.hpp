#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/SlotMapHandle.hpp>

#include <cppcoro/generator.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>

namespace Cory {
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
template <typename StoredType_> class SlotMap : NoCopy {
  public:
    using StoredType = StoredType_;
    static constexpr size_t CHUNK_SIZE{64};

    SlotMap() = default;
    ~SlotMap();

    // movable
    SlotMap(SlotMap &&) = default;
    SlotMap &operator=(SlotMap &&) = default;

    /// access to an element via its handle - throws if handle is not valid
    [[nodiscard]] StoredType &operator[](SlotMapHandle id);
    /// const access to an element via its handle - throws if handle is not valid
    [[nodiscard]] const StoredType &operator[](SlotMapHandle id) const;

    /// create a new element in-place
    template <typename... InitArgs> SlotMapHandle emplace(InitArgs... args);

    /// insert a default-constructed element
    SlotMapHandle insert() { return emplace(); }
    /// insert an rvalue (move into the slotmap)
    SlotMapHandle insert(StoredType &&value) { return emplace(std::move(value)); }
    /// insert a copy of an element
    SlotMapHandle insert(const StoredType &value) { return emplace(value); }

    /// release the object, invalidating previous handles and reclaiming the memory for future use
    void release(SlotMapHandle id);

    /// release all objects from the slot map
    void clear();

    /// update by assigning a new value, will invalidate old handles to the entry
    template <typename ArgumentType> SlotMapHandle update(SlotMapHandle id, ArgumentType &&value);

    /**
     * invalidate the old handles, internally increasing the version of the object.
     * use to reflect a semantic change in the value
     */
    SlotMapHandle update(SlotMapHandle id);

    /// get number of alive elements in the slot map
    [[nodiscard]] size_t size() const noexcept
    {
        return chunkTable_.size() * CHUNK_SIZE - freeList_.size();
    }
    /// get number of currently allocated slots
    [[nodiscard]] size_t capacity() const noexcept { return chunkTable_.size() * CHUNK_SIZE; }

    /// query if the slot map is empty
    [[nodiscard]] bool empty() const { return size() == 0;}

    /// check if a given handle is valid to dereference
    [[nodiscard]] bool isValid(SlotMapHandle id) const;

    struct sentinel {}; // sentinel type for end()
    template <bool IsConst> struct iterator_base : public std::forward_iterator_tag {
        using SlotMapType = std::conditional_t<IsConst, const SlotMap *, SlotMap *>;
        using DereferencedType = std::conditional_t<IsConst, const StoredType &, StoredType &>;
        using difference_type = int32_t;
        using value_type = StoredType;
        using reference = uint32_t &;
        using pointer = uint32_t *;

        iterator_base(SlotMapType sm, uint32_t index)
            : sm_{sm}
            , index_{index}
        {
        }
        bool operator==(sentinel) const { return index_ == sm_->capacity(); }
        iterator_base &operator++()
        {
            index_ = sm_->findNextAliveIndex(index_ + 1);
            return *this;
        }
        iterator_base &operator++(int)
        {
            index_ = sm_->findNextAliveIndex(index_ + 1);
            return *this;
        }
        DereferencedType operator*()
        {
            auto object = sm_->objectAt(index_);
            return const_cast<DereferencedType>(object.storage);
        }

      private:
        SlotMapType sm_;
        uint32_t index_{};
    };
    using iterator = iterator_base<false>;
    using const_iterator = iterator_base<true>;

    iterator begin() noexcept { return {this, findNextAliveIndex(0)}; }
    const_iterator begin() const noexcept { return {this, findNextAliveIndex(0)}; }
    const_iterator cbegin() const noexcept { return {this, findNextAliveIndex(0)}; }
    constexpr sentinel end() const noexcept { return {}; }
    constexpr sentinel cend() const noexcept { return {}; }

    /// generator to iterate over all alive handles
    cppcoro::generator<SlotMapHandle> handles() const;
    /// generator to iterate over all alive handles together with their values
    cppcoro::generator<std::pair<SlotMapHandle, StoredType &>> items();
    cppcoro::generator<std::pair<SlotMapHandle, const StoredType &>> items() const;

  private:
    struct Chunk {
        SlotMapHandle id[CHUNK_SIZE];
        StoredType storage[CHUNK_SIZE];
    };
    struct StoredInner {
        SlotMapHandle &id;
        StoredType &storage;
    };
    struct ConstStoredInner {
        const SlotMapHandle &id;
        const StoredType &storage;
    };

    StoredInner objectAt(uint32_t index);
    ConstStoredInner objectAt(uint32_t index) const;

    StoredInner validatedGet(SlotMapHandle handle);

    /**
     * find the index of the next alive object in the SlotMap at or after @a start
     * Will return SlotMap::capacity() if no alive element exists after @a start
     */
    uint32_t findNextAliveIndex(uint32_t start = 0) const;

  private:
    std::allocator<Chunk> alloc_;
    std::vector<Chunk *> chunkTable_;
    std::vector<uint32_t> freeList_;
};

/**
 * convenience handle - less memory efficient but more convenient - stores the slotmap in order
 * to resolve the handle when needed
 */
template <typename StoredType> class ResolvableHandle {
  public:
    ResolvableHandle(SlotMap<StoredType> &slotMap, SlotMapHandle handle)
        : slotMap_{&slotMap}
        , handle_{handle}
    {
    }
    ResolvableHandle(const ResolvableHandle &) = default;
    ResolvableHandle(ResolvableHandle &&) = default;
    ResolvableHandle &operator=(const ResolvableHandle &) = default;
    ResolvableHandle &operator=(ResolvableHandle &&) = default;

    StoredType &operator*() { return (*slotMap_)[handle_]; }
    const StoredType &operator*() const { return (*slotMap_)[handle_]; }

    StoredType *operator->() { return &(*slotMap_)[handle_]; }
    const StoredType *operator->() const { return &(*slotMap_)[handle_]; }

    SlotMap<StoredType> &slotMap() { return (*slotMap_); }
    const SlotMap<StoredType> &slotMap() const { return (*slotMap_); }
    /* implicit */ operator SlotMapHandle() { return handle_; }
    SlotMapHandle handle() const { return handle_; }

    bool valid() const { return slotMap_->isValid(handle_); }

    auto operator<=>(const ResolvableHandle &) const noexcept = default;

  private:
    SlotMap<StoredType> *slotMap_;
    SlotMapHandle handle_;
};

} // namespace Cory

/// make ResolvableHandle hashable
template <typename StoredType> struct std::hash<Cory::ResolvableHandle<StoredType>> {
    std::size_t operator()(const Cory::ResolvableHandle<StoredType> &s) const noexcept
    {
        return Cory::hashCompose(0, s.handle(), &s.slotMap());
    }
};

// ======================================== Implementation ========================================

namespace Cory {
template <typename StoredType_> StoredType_ &SlotMap<StoredType_>::operator[](SlotMapHandle id)
{
    auto object = validatedGet(id);
    return object.storage;
}

template <typename StoredType_>
const StoredType_ &SlotMap<StoredType_>::operator[](SlotMapHandle id) const
{
    return const_cast<SlotMap &>(*this)[id];
}

template <typename StoredType_>
template <typename... InitArgs>
SlotMapHandle SlotMap<StoredType_>::emplace(InitArgs... args)
{
    // check if we need to allocate more chunks
    if (freeList_.empty()) {
        // allocate a new chunk and add the leftovers to the free list
        const auto num_chunks = chunkTable_.size();
        Chunk &chunk = *chunkTable_.emplace_back(alloc_.allocate(1));

        freeList_.reserve(CHUNK_SIZE);

        for (uint64_t i = CHUNK_SIZE - 1; i > 0; --i) {
            const auto idx = num_chunks * CHUNK_SIZE + i;
            // insert handles with the 'free' bit set
            chunk.id[i] = SlotMapHandle{static_cast<uint32_t>(idx), 0, true};
            freeList_.push_back(static_cast<uint32_t>(idx));
        }
        const auto idx = num_chunks * CHUNK_SIZE;
        chunk.id[0] = SlotMapHandle{static_cast<uint32_t>(idx), 0, false};

        try {
            new (&chunk.storage[0]) StoredType{std::forward<InitArgs>(args)...};
        }
        catch (...) {
            // if construction throws, put slot back into free list
            chunk.id[0] = SlotMapHandle::setFreeBit(chunk.id[0]);
            freeList_.push_back(chunk.id[0].index());
            throw;
        }

        return chunk.id[0];
    }

    // get the next free index
    int free = freeList_.back();
    freeList_.pop_back();

    auto object = objectAt(free);
    CO_CORE_ASSERT(!object.id.valid(), "We got a live object from the free list!");

    try {
        // try to construct the object in the storage
        new (&object.storage) StoredType{std::forward<InitArgs>(args)...};
    }
    catch (...) {
        // if construction throws, put slot back into free list
        freeList_.push_back(object.id.index());
        throw;
    }
    object.id = SlotMapHandle::clearFreeBit(object.id);

    return object.id;
}

template <typename StoredType_> void SlotMap<StoredType_>::release(SlotMapHandle id)
{
    auto object = validatedGet(id);
    // increase version and set free bit
    object.id = SlotMapHandle::setFreeBit(SlotMapHandle::nextVersion(id));

    freeList_.push_back(id.index());
    object.storage.~StoredType(); // destruct the object
}

template <typename StoredType_> void SlotMap<StoredType_>::clear()
{
    // call destructor on remaining objects if they are still alive
    for (auto &chunkPtr : chunkTable_) {
        auto &chunk = *chunkPtr;
        for (gsl::index i = 0; i < CHUNK_SIZE; ++i) {
            const auto objectId = chunk.id[i];
            // if it's alive, destroy it and put into free list
            if (objectId.valid()) {
                if constexpr (!std::is_trivial_v<StoredType>) { chunk.storage[i].~StoredType(); }
                // clear free bit and increase version
                chunk.id[i] = SlotMapHandle::setFreeBit(SlotMapHandle::nextVersion(objectId));
                freeList_.push_back(chunk.id[i].index());
            }
        }
    }
}

template <typename StoredType_>
template <typename ArgumentType>
SlotMapHandle SlotMap<StoredType_>::update(SlotMapHandle id, ArgumentType &&value)
{
    auto object = validatedGet(id);
    object.storage = value; // update stored value
    object.id = SlotMapHandle::nextVersion(id);
    return object.id;
}

template <typename StoredType_> SlotMapHandle SlotMap<StoredType_>::update(SlotMapHandle id)
{
    auto object = validatedGet(id);
    object.id = SlotMapHandle::nextVersion(id); // increase version
    return object.id;
}

template <typename StoredType_> bool SlotMap<StoredType_>::isValid(SlotMapHandle id) const
{
    if (!id.valid()) { return false; }

    const uint32_t chunkIndex = id.index() / CHUNK_SIZE;
    if (chunkIndex >= chunkTable_.size()) { return false; }
    auto object = objectAt(id.index());
    return object.id == id;
}

template <typename StoredType_> SlotMap<StoredType_>::~SlotMap()
{
    if constexpr (!std::is_trivial_v<StoredType>) { clear(); }
    for (Chunk *chunkPtr : chunkTable_) {
        alloc_.deallocate(chunkPtr, 1);
    }
}

template <typename StoredType_>
typename SlotMap<StoredType_>::StoredInner SlotMap<StoredType_>::objectAt(uint32_t index)
{
    const uint32_t chunkIndex = index / CHUNK_SIZE;
    const uint32_t elementIndex = index % CHUNK_SIZE;

    auto &chunk = *chunkTable_[chunkIndex];
    return StoredInner{.id = chunk.id[elementIndex], .storage = chunk.storage[elementIndex]};
}
template <typename StoredType_>
typename SlotMap<StoredType_>::ConstStoredInner SlotMap<StoredType_>::objectAt(uint32_t index) const
{
    const uint32_t chunkIndex = index / CHUNK_SIZE;
    const uint32_t elementIndex = index % CHUNK_SIZE;

    auto &chunk = *chunkTable_[chunkIndex];
    return ConstStoredInner{.id = chunk.id[elementIndex], .storage = chunk.storage[elementIndex]};
}

template <typename StoredType_>
typename SlotMap<StoredType_>::StoredInner SlotMap<StoredType_>::validatedGet(SlotMapHandle handle)
{
    if (!handle.valid()) {
        throw std::runtime_error{"Given handle is dead or invalid."}; //
    }

    if (handle.index() >= capacity()) {
        throw std::runtime_error{fmt::format(
            "Handle index {} is out of range ({}). Is the handle really from this slot map?!",
            handle.index(),
            size())};
    }
    const uint32_t chunkIndex = handle.index() / CHUNK_SIZE;
    const uint32_t elementIndex = handle.index() % CHUNK_SIZE;

    auto &chunk = *chunkTable_[chunkIndex];
    const auto objectId = chunk.id[elementIndex];
    if (objectId != handle) {
        //        CO_CORE_ASSERT(chunk.id[elementIndex] == handle.index(),
        //                       "Object index mismatch ({} != {}) - object is in the wrong place!",
        //                       objectId.index(),
        //                       handle.index());
        throw std::runtime_error{
            fmt::format("Handle is outdated. Object version = {} but handle version = {}",
                        objectId.version(),
                        handle.version())};
    }
    return {chunk.id[elementIndex], chunk.storage[elementIndex]};
}

template <typename StoredType_>
uint32_t SlotMap<StoredType_>::findNextAliveIndex(uint32_t start) const
{
    uint32_t cap = static_cast<uint32_t>(capacity());
    for (uint32_t index = start; index < cap; ++index) {
        ConstStoredInner object = objectAt(index);
        if (object.id.valid()) { return index; }
    }
    return cap;
}

template <typename StoredType_>
cppcoro::generator<SlotMapHandle> SlotMap<StoredType_>::handles() const
{
    for (const auto &chunkPtr : chunkTable_) {
        auto &chunk = *chunkPtr;
        for (gsl::index i = 0; i < CHUNK_SIZE; ++i) {
            auto handle = chunk.id[i];
            if (handle.valid()) { co_yield handle; }
        }
    }
}

template <typename StoredType_>
cppcoro::generator<std::pair<SlotMapHandle, StoredType_ &>> SlotMap<StoredType_>::items()
{
    for (auto &chunkPtr : chunkTable_) {
        auto &chunk = *chunkPtr;
        for (gsl::index i = 0; i < CHUNK_SIZE; ++i) {
            if (chunk.id[i].valid()) {
                co_yield std::make_pair(chunk.id[i], std::ref(chunk.storage[i]));
            }
        }
    }
}
template <typename StoredType_>
cppcoro::generator<std::pair<SlotMapHandle, const StoredType_ &>>
SlotMap<StoredType_>::items() const
{
    for (auto &chunkPtr : chunkTable_) {
        auto &chunk = *chunkPtr;
        for (gsl::index i = 0; i < CHUNK_SIZE; ++i) {
            if (chunk.id[i].valid()) {
                co_yield std::make_pair(chunk.id[i], std::ref(chunk.storage[i]));
            }
        }
    }
}

} // namespace Cory