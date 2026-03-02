#pragma once

#include <Cory/Base/Common.hpp>

#include <mutex>

namespace Cory {

/// @brief A simple wrapper that provides locked access to an object of type T
///
/// @tparam T The type of the object to be locked
///
/// Usage:
/// ```cpp
///     Locked<MyType> lockedObject{MyType{}};
///     {
///         auto data = lockedObject.lock();
///         data->doSomething();
///     } // lock is released here
/// ```
///
template <typename T> class Locked : NoCopy, NoMove {
  public:
    // Proxy to ensure correct locking/unlocking when accessing the object
    class LockedProxy : NoCopy, NoMove {
      public:
        LockedProxy(T *object, std::mutex &mutex)
            : lock_(mutex)
            , object_(object)
        {
        }
        T *operator->() { return object_; }
        T &operator*() { return *object_; }

      private:
        std::unique_lock<std::mutex> lock_;
        T *object_;
    };
    // Proxy to ensure correct const locking/unlocking when accessing the object
    class ConstLockedProxy : NoCopy, NoMove {
      public:
        ConstLockedProxy(const T *object, std::mutex &mutex)
            : lock_(mutex)
            , object_(object)
        {
        }
        const T *operator->() { return object_; }
        const T &operator*() { return *object_; }

      private:
        std::unique_lock<std::mutex> lock_;
        const T *object_;
    };

    Locked() = default;

    explicit Locked(T object)
        : object_(std::move(object))
    {
    }

    // movable
    Locked(Locked &&) = default;
    Locked &operator=(Locked &&) = default;

    auto lock() { return LockedProxy{&object_, mutex_}; }
    auto lock() const { return ConstLockedProxy{&object_, mutex_}; }

    auto exchange(T newObject)
    {
        std::scoped_lock lock(mutex_);
        return std::exchange(object_, newObject);
    }

  private:
    T object_;
    mutable std::mutex mutex_;
};

} // namespace Cory
