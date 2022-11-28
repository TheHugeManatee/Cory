#pragma once

#include <Cory/Base/Log.hpp>

#include <cppcoro/coroutine.hpp>
#include <cppcoro/is_awaitable.hpp>

#include <type_traits>

namespace Cory::Framegraph {

/**
 * An async render pass declaration awaitable that shall be used as a return type to declare a
 * render pass from a coroutine.
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
            CO_CORE_ASSERT(!outputsProvided_,
                           "Coroutine cannot yield multiple RenderPassOutput structs!");
            output_ = std::move(output);
            outputsProvided_ = true;
            return {};
        }
        void return_void() {}

        RenderPassOutput output_;
        bool outputsProvided_{false};
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
        // todo: if a coroutine never calls builder.finishDeclaration(), its coroutine handle will
        // leak! :/
        // if (coroHandle_) { coroHandle_.destroy(); }
    }

    const RenderPassOutput &output()
    {
        // only resume the coroutine if it did not yet yield an output
        if (!coroHandle_.promise().outputsProvided_ && !coroHandle_.done()) {
            coroHandle_.resume();
        }
        CO_CORE_ASSERT(coroHandle_.promise().outputsProvided_,
                       "Render pass coroutine did not yield an outputs struct!");

        return coroHandle_.promise().output_;
    }

  private:
    Handle coroHandle_;
};

} // namespace Cory::Framegraph