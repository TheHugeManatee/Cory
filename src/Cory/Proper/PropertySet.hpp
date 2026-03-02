#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Proper/Properties.hpp>

#include <cppcoro/coroutine.hpp>

#include <unordered_map>
#include <utility>
#include <vector>

namespace Cory {

class PropertySet {
  public:
    /// Create a new property
    PropertyHandle create(const std::string &name, IsProperty auto property);
    /// create a property with a given value and automatically chosen min/max/step if applicable
    PropertyHandle create(const std::string &name, PropertyValue value);
    /// Create a new group
    GroupHandle addGroup(const std::string &name);

    /// Update a value. resumes any pending actions on the property
    void update(PropertyHandle handle, PropertyValue value);

    PropertyHandle find(const std::string &name);
    PropertyValue read(PropertyHandle handle);

    struct PropertyProxy {
        PropertyHandle handle;
        PropertySet *set;

        template <IsPropertyValue ValueType> void operator=(ValueType value)
        {
            set->update(handle, value);
        }

        PropertyValue operator()() const { return set->read(handle); }

        auto changed() const { return set->changed(handle); }
        template <IsPropertyValue ValueType> auto changed() const
        {
            return set->changed<ValueType>(handle);
        }
    };
    PropertyProxy operator[](PropertyHandle handle);

    class ChangedAwaiter {
      public:
        ChangedAwaiter(PropertySet &set, PropertyHandle handle)
            : set_{set}
            , handle_{handle}
        {
        }

        bool await_ready() const noexcept { return false; }
        template <typename PromiseType>
        void await_suspend(cppcoro::coroutine_handle<PromiseType> awaiting)
        {
            auto awaiterHandle = cppcoro::coroutine_handle<>{awaiting};
            set_.registerWaiter(handle_, awaiterHandle);

            if constexpr (std::same_as<PromiseType, Task<void>::promise_type>) {
                awaiting.promise().setCancellation(&set_, handle_, awaiterHandle);
            }
        }
        void await_resume() const noexcept {}

      private:
        PropertySet &set_;
        PropertyHandle handle_;
    };

    template <IsPropertyValue ValueType> class TypedChangedAwaiter {
      public:
        TypedChangedAwaiter(PropertySet &set, PropertyHandle handle)
            : set_{set}
            , handle_{handle}
        {
        }

        bool await_ready() const noexcept { return false; }
        template <typename PromiseType>
        void await_suspend(cppcoro::coroutine_handle<PromiseType> awaiting)
        {
            auto awaiterHandle = cppcoro::coroutine_handle<>{awaiting};
            set_.registerWaiter(handle_, awaiterHandle);

            if constexpr (std::same_as<PromiseType, Task<void>::promise_type>) {
                awaiting.promise().setCancellation(&set_, handle_, awaiterHandle);
            }
        }
        ValueType await_resume() const
        {
            const auto value = set_.read(handle_);
            CO_CORE_DEBUG_ASSERT(std::holds_alternative<ValueType>(value),
                                 "Property type mismatch in changed<T>()");
            return std::get<ValueType>(value);
        }

      private:
        PropertySet &set_;
        PropertyHandle handle_;
    };

  private:
    friend class Task<void>::promise_type;

    PropertyHandle createImpl(const std::string &name, PropertyVariant property);
    ChangedAwaiter changed(PropertyHandle handle);
    template <IsPropertyValue ValueType>
    TypedChangedAwaiter<ValueType> changed(PropertyHandle handle)
    {
        return TypedChangedAwaiter<ValueType>{*this, handle};
    }
    void registerWaiter(PropertyHandle handle, cppcoro::coroutine_handle<> awaiting);
    void unregisterWaiter(PropertyHandle handle, cppcoro::coroutine_handle<> awaiting);
    void notifyChanged(PropertyHandle handle);

    SlotMap<PropertyVariant> properties_;
    SlotMap<Group> groups_;
    std::unordered_map<std::string, PropertyHandle> propertyLookup_;
    std::unordered_map<std::string, GroupHandle> groupLookup_;
    std::unordered_map<PropertyHandle, std::vector<cppcoro::coroutine_handle<>>> waiters_;
};

// =============================== Implementation ====================================== //

PropertyHandle PropertySet::create(const std::string &name, IsProperty auto property)
{
    return createImpl(name, PropertyVariant{std::move(property)});
}
} // namespace Cory
