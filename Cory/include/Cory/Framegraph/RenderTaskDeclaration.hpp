#pragma once

#include <Cory/Base/Log.hpp>

#include <cppcoro/coroutine.hpp>
#include <cppcoro/is_awaitable.hpp>

#include <type_traits>

namespace Cory {

/**
 * An async render task declaration awaitable that shall be used as a return type to declare a
 * render task from a coroutine.
 */
template <typename RenderTaskOutput> class RenderTaskDeclaration {
  public:
    struct promise_type {
        cppcoro::suspend_always initial_suspend() noexcept { return {}; }

        RenderTaskDeclaration get_return_object() { return RenderTaskDeclaration{this}; }

        cppcoro::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception()
        {
            try {
                std::rethrow_exception(std::current_exception());
            }
            catch ([[maybe_unused]] const std::exception &e) {
                CO_CORE_TRACE("Unhandled exception in coroutine: {}", e.what());
            }
            exception_ = std::current_exception();
        }

        cppcoro::suspend_never yield_value(RenderTaskOutput output)
        {
            CO_CORE_ASSERT(!outputsProvided_,
                           "Coroutine cannot yield multiple RenderTaskOutput structs!");
            output_ = std::move(output);
            outputsProvided_ = true;
            return {};
        }
        void return_void() {}

        // todo: this could easily be a std::variant
        RenderTaskOutput output_;
        std::exception_ptr exception_{nullptr};
        bool outputsProvided_{false};
    };

    using Handle = cppcoro::coroutine_handle<promise_type>;

    explicit RenderTaskDeclaration(promise_type *p)
        : coroHandle_{Handle::from_promise(*p)}
    {
    }
    // move-only!
    RenderTaskDeclaration(RenderTaskDeclaration &&rhs)
        : coroHandle_{std::exchange(rhs.coroHandle_, nullptr)}
    {
    }
    ~RenderTaskDeclaration()
    {
        // todo: if a coroutine never calls builder.finishDeclaration(), its coroutine handle will
        // leak! :/
        // if (coroHandle_) { coroHandle_.destroy(); }
    }

    const RenderTaskOutput &output()
    {
        // only resume the coroutine if it did not yet yield an output
        if (!coroHandle_.promise().outputsProvided_ && !coroHandle_.done()) {
            coroHandle_.resume();
        }

        // if necessary, rethrow the exception raised by the coroutine
        if (auto ex = coroHandle_.promise().exception_; ex != nullptr) {
            std::rethrow_exception(ex);
        }
        CO_CORE_ASSERT(coroHandle_.promise().outputsProvided_,
                       "Render pass coroutine did not yield an outputs struct!");

        return coroHandle_.promise().output_;
    }

  private:
    Handle coroHandle_;
};

} // namespace Cory