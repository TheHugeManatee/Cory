#pragma once

#include <cppcoro/coroutine.hpp>
#include <cppcoro/is_awaitable.hpp>

#include <type_traits>

namespace Cory::Framegraph {

/**
 * An async render pass declaration that shall be used as a return type to declare a render pass
 * from a coroutine.
 */
template <typename RenderPassOutput> class RenderPassDeclaration {
  public:
    struct promise_type {
        cppcoro::suspend_always initial_suspend() noexcept { return {}; }

        RenderPassDeclaration get_return_object() { return RenderPassDeclaration{this}; }

        cppcoro::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() {}

        cppcoro::suspend_never yield_value(RenderPassOutput output)
        {
            output_ = output;
            return {};
        }
        void return_void() {}

        //        // support co_await'ing for a std::string{} as well as a general awaitable
        //        template <typename T> decltype(auto) await_transform(T &&t)
        //        {
        //            if constexpr (std::is_same_v<T, RenderInput>) {
        //                struct awaiter {
        //                    promise_type &pt;
        //                    constexpr bool await_ready() const noexcept { return true; }
        //                    RenderInput await_resume() const noexcept { return
        //                    std::move(pt.renderInput); } void
        //                    await_suspend(cppcoro::coroutine_handle<promise_type>) const noexcept
        //                    {}
        //                };
        //                return awaiter{*this};
        //            }
        //            else if constexpr (cppcoro::is_awaitable_v<T>) {
        //                return t.operator co_await();
        //            }
        //        }

        RenderPassOutput output_;
    };

    using Handle = cppcoro::coroutine_handle<promise_type>;

    explicit RenderPassDeclaration(promise_type *p)
        : coroHandle_{Handle::from_promise(*p)}
    {
    }
    // move-only!
    RenderPassDeclaration(RenderPassDeclaration &&rhs)
        : coroHandle_{std::exchange(rhs.coroHandle_, nullptr)}
    {
    }
    ~RenderPassDeclaration()
    {
        //if (coroHandle_) { coroHandle_.destroy(); }
    }

    RenderPassOutput output()
    {
        if (!coroHandle_.done()) { coroHandle_.resume(); }

        return std::move(coroHandle_.promise().output_);
    }

  private:
    Handle coroHandle_;
};

} // namespace Cory::Framegraph