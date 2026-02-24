#pragma once

#include <memory>
#include <utility>

namespace Cory {

template <typename Signature> class Function;

// Check if a callable is invocable with given arguments
template <typename F, typename... Args>
concept ConstCallableWith = requires(const F &f, Args &&...args) {
    { f(std::forward<Args>(args)...) };
};

/**
 * @brief Minimal std::function replacement to avoid #include <functional>
 */
template <typename R, typename... Args> class Function<R(Args...)> {
    struct CallableBase {
        virtual R invoke(Args... args) const = 0;
        virtual ~CallableBase() = default;
    };

    template <typename F> struct CallableImpl : CallableBase {
        F f;

        CallableImpl(F &&function)
            : f(std::forward<F>(function))
        {
        }

        R invoke(Args... args) const override
        {
            if constexpr (ConstCallableWith<F, Args...>) {
                return f(std::forward<Args>(args)...);
            }
            else {
                // f might be a mutable lambda, so we have to const_cast here
                return const_cast<F &>(f)(std::forward<Args>(args)...);
            }
        }
    };

    std::shared_ptr<CallableBase> callable;

  public:
    template <typename F>
    Function(F &&f)
        : callable(std::make_unique<CallableImpl<std::decay_t<F>>>(std::forward<F>(f)))
    {
    }

    Function() = default;

    explicit operator bool() const { return callable != nullptr; }

    R operator()(Args... args) const { return callable->invoke(std::forward<Args>(args)...); }
};

} // namespace Cory
