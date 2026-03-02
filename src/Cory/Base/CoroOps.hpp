#pragma once

#include <Cory/Base/CoroThreadPool.hpp>

#include <cppcoro/awaitable_traits.hpp>
#include <cppcoro/detail/remove_rvalue_reference.hpp>
#include <cppcoro/task.hpp>

#include <type_traits>
#include <utility>

namespace Cory {

namespace detail {

template <
    typename TStoredAwaitable,
    typename TResult = typename cppcoro::awaitable_traits<TStoredAwaitable &&>::await_result_t,
    typename TReturn = typename cppcoro::detail::remove_rvalue_reference<TResult>::type>
auto resume_on_impl(CoroThreadPool &pool, TStoredAwaitable awaitable) -> cppcoro::task<TReturn>
{
    co_await pool.schedule();
    if constexpr (std::is_void_v<TReturn>) {
        co_await std::move(awaitable);
        co_return;
    }
    else {
        co_return co_await std::move(awaitable);
    }
}

} // namespace detail

template <typename TAwaitable> auto resume_on(CoroThreadPool &pool, TAwaitable &&awaitable)
{
    using TStoredAwaitable = std::decay_t<TAwaitable>;
    return detail::resume_on_impl(pool, TStoredAwaitable{std::forward<TAwaitable>(awaitable)});
}

} // namespace Cory
