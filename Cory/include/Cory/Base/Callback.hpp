#pragma once

#include <concepts>
#include <functional>
#include <mutex>

namespace Cory {

// clang-format off
template <typename Callable, typename... Args>
concept CompatibleCallable = requires(Callable cb) {
    { cb(std::declval<Args>()...) } -> std::same_as<void>;
};
// clang-format on

template <typename... Args> class Callback {
  public:
    Callback() = default;
    Callback(Callback &&rhs) noexcept { rhs.swap(*this); }
    Callback &operator=(Callback &&rhs) noexcept
    {
        if (this != &rhs) { rhs.swap(*this); }
        return *this;
    }

    void swap(Callback &rhs) noexcept
    {
        std::lock(mtx_, rhs.mtx_);
        std::swap(rhs.cb_, cb_);
        mtx_.unlock();
        rhs.mtx_.unlock();
    }

    /// invoke the callback function, if one is registered
    void invoke(Args... args) const
    {
        std::function<void(Args...)> fn;
        {
            std::lock_guard lk{mtx_};
            fn = cb_;
        }
        if (fn) { fn(args...); }
    }

    /// register a new callback function, replacing any previously registered functions
    template <typename Callable>
        requires CompatibleCallable<Callable, Args...>
    void operator()(Callable &&callable)
    {
        std::lock_guard lk{mtx_};
        cb_ = std::move(callable);
    }

    /// reset the callback function, removing any previously registered callback function
    void reset()
    {
        std::lock_guard lk_{mtx_};
        cb_ = {};
    }

  private:
    mutable std::mutex mtx_;
    std::function<void(Args...)> cb_;
};

template <> class Callback<void> {
  public:
    using ReturnType = void;

    Callback() = default;
    Callback(Callback &&rhs) noexcept { rhs.swap(*this); }
    Callback &operator=(Callback &&rhs) noexcept
    {
        if (this != &rhs) { rhs.swap(*this); }
        return *this;
    }
    void swap(Callback &rhs) noexcept
    {
        std::lock(mtx_, rhs.mtx_);
        std::swap(rhs.cb_, cb_);
        mtx_.unlock();
        rhs.mtx_.unlock();
    }

    /// invoke the callback function, if one is registered
    void invoke() const
    {
        std::function<void()> fn;
        {
            std::lock_guard lk{mtx_};
            fn = cb_;
        }
        if (fn) { fn(); }
    }

    /// register a new callback function, replacing any previously registered functions
    void operator()(auto &&callable) { cb_ = std::move(callable); }

    /// reset the callback function, removing any previously registered callback function
    void reset()
    {
        std::unique_lock lk_{mtx_};
        cb_ = {};
    }

  private:
    mutable std::mutex mtx_;
    std::function<void()> cb_;
};

// template <typename Return> class Callback<Return> {
//   public:
//     using ReturnType = Return;
//
//     Callback operator()()
//     {
//         if (cb_) { cb_(); }
//     }
//
//     template <typename Callable> Callback operator()(CompatibleCallable<Callback> auto
//     &&callable)
//     {
//         cb_ = std::move(callable);
//     }
//
//   private:
//     std::function<Return()> cb_;
// }

} // namespace Cory