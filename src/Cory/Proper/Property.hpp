#pragma once

#include "Properties.hpp"

#include <cppcoro/coroutine.hpp>

#include <concepts>
#include <vector>

namespace Cory {

template <std::copyable T> class Property : public AbstractProperty {
  public:
    Property() = default;
    explicit Property(T initialValue);

    auto get() const -> const T &;
    auto operator()() const -> T;

    auto set(const T &newValue) -> void;
    template <typename U = T> auto set(U &&newValue) -> void;
    auto operator=(const T &newValue) -> Property &;
    template <typename U = T> auto operator=(U &&newValue) -> Property &;

    class ChangedAwaiter {
      public:
        ChangedAwaiter(Property *property);

        auto await_ready() const noexcept -> bool;
        template <typename PromiseType>
        auto await_suspend(cppcoro::coroutine_handle<PromiseType> awaiting) -> void;
        auto await_resume() const -> T;

      private:
        Property *property_;
    };

    auto changed() -> ChangedAwaiter;

  private:
    T value;
};

// =============================== Implementation ====================================== //

template <std::copyable T>
Property<T>::Property(T initialValue)
    : value(std::move(initialValue))
{
}
template <std::copyable T> const T &Property<T>::get() const
{
    return value;
}

template <std::copyable T> void Property<T>::set(const T &newValue)
{
    value = newValue;
    notifyWaiters();
}

template <std::copyable T> template <typename U> void Property<T>::set(U &&newValue)
{
    value = std::forward<U>(newValue);
    notifyWaiters();
}

template <std::copyable T> T Property<T>::operator()() const
{
    return get();
}

template <std::copyable T> Property<T> &Property<T>::operator=(const T &newValue)
{
    set(newValue);
    return *this;
}

template <std::copyable T> template <typename U> Property<T> &Property<T>::operator=(U &&newValue)
{
    set(std::forward<U>(newValue));
    return *this;
}

template <std::copyable T>
Property<T>::ChangedAwaiter::ChangedAwaiter(Property *property)
    : property_{property}
{
}

template <std::copyable T> bool Property<T>::ChangedAwaiter::await_ready() const noexcept
{
    return false;
}

template <std::copyable T>
template <typename PromiseType>
void Property<T>::ChangedAwaiter::await_suspend(cppcoro::coroutine_handle<PromiseType> awaiting)
{
    auto awaiterHandle = cppcoro::coroutine_handle<>{awaiting};
    property_->registerWaiter(awaiterHandle);

    if constexpr (std::same_as<PromiseType, Task<void>::promise_type>) {
        awaiting.promise().setCancellation(property_, awaiterHandle);
    }
}

template <std::copyable T> T Property<T>::ChangedAwaiter::await_resume() const
{
    return property_->get();
}

template <std::copyable T> typename Property<T>::ChangedAwaiter Property<T>::changed()
{
    return {this};
}

} // namespace Cory
