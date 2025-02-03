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

    R operator()(Args... args) { return callable->invoke(std::forward<Args>(args)...); }
};

template <typename Signature> class ContractFunctor;

template <typename OptionalArg> class ContractFunctor {
    struct CallableBase {
        virtual void invoke(OptionalArg arg) = 0;
        virtual ~CallableBase() = default;
    };

    template <typename F> struct CallableImpl : CallableBase {
        F f;

        CallableImpl(F &&f)
            : f(std::forward<F>(f))
        {
        }

        void invoke(OptionalArg arg) override
        {
            if constexpr (std::is_invocable_v<F, OptionalArg>) { f(arg); }
            else {
                f();
            }
        }
    };

    std::unique_ptr<CallableBase> callable;

  public:
    template <typename F>
    ContractFunctor(F &&f)
        : callable(std::make_unique<CallableImpl<std::decay_t<F>>>(std::forward<F>(f)))
    {
    }

    void operator()(OptionalArg arg) { return callable->invoke(std::forward<OptionalArg>(arg)); }
};

} // namespace Cory
