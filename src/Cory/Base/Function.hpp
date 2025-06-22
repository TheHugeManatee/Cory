#pragma once

#include <memory>
#include <utility>

namespace Cory {

template <typename Signature> class Function;

template <typename R, typename... Args> class Function<R(Args...)> {
    struct CallableBase {
        virtual R invoke(Args... args) = 0;
        virtual ~CallableBase() = default;
    };

    template <typename F> struct CallableImpl : CallableBase {
        F f;

        CallableImpl(F &&f)
            : f(std::forward<F>(f))
        {
        }

        R invoke(Args... args) override { return f(std::forward<Args>(args)...); }
    };

    std::unique_ptr<CallableBase> callable;

  public:
    template <typename F>
    Function(F &&f)
        : callable(std::make_unique<CallableImpl<std::decay_t<F>>>(std::forward<F>(f)))
    {
    }

    Function() = default;

    explicit operator bool() const { return callable != nullptr; }

    R operator()(Args... args) { return callable->invoke(std::forward<Args>(args)...); }
};

} // namespace Cory
