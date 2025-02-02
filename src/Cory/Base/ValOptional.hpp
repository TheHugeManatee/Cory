#include <concepts>
#include <limits>
#include <stdexcept>

namespace Cory {

struct BadValueOptional : std::runtime_error {
    BadValueOptional()
        : std::runtime_error{"Bad ValueOptional access"}
    {
    }
};

/**
 * @brief An alternative class to std::optional that uses a special value to indicate emptyness.
 * @tparam T The type to use
 * @tparam INVALID The invalid value to use. Defaults to std::numeric_limits<T>::max()
 */
template <std::regular T, T INVALID = std::numeric_limits<T>::max()> class ValOptional {
  public:
    static constexpr T InvalidValue = INVALID;

    constexpr ValOptional() = default;
    constexpr /*implicit*/ ValOptional(std::nullopt_t) {}

    constexpr /*implicit*/ ValOptional(T value)
        : value_{value}
    {
    }

    constexpr ValOptional(ValOptional &other) = default;
    constexpr ValOptional(ValOptional &&other) = default;
    constexpr ValOptional &operator=(ValOptional &other) = default;
    constexpr ValOptional &operator=(ValOptional &&other) = default;

    [[nodiscard]] constexpr bool has_value() const { return value_ != InvalidValue; }

    explicit constexpr operator bool() const { return has_value(); }

    [[nodiscard]] constexpr T value() const
    {
        if (value_ == InvalidValue) { throw BadValueOptional{}; }
        return value_;
    }

    [[nodiscard]] constexpr T value_or(T defaultValue) const
    {
        return has_value() ? value_ : defaultValue;
    }

    [[nodiscard]] constexpr T operator*() const { return value_; }

    [[nodiscard]] constexpr bool operator==(const ValOptional &other) const
    {
        return value_ == other.value_;
    }

    template <typename F, typename R = std::remove_cvref_t<std::invoke_result_t<F, T>>>
    [[nodiscard]] ValOptional<R> and_then(F &&f)
    {
        if (has_value()) { return std::invoke(std::forward<F>(f), value_); }
        return ValOptional<R>{};
    }

  private:
    T value_{InvalidValue};
};

} // namespace Cory
