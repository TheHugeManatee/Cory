#pragma once

#include "Property.hpp"

#include <glm/common.hpp>
#include <glm/vector_relational.hpp>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Cory {

namespace detail {

template <typename T>
concept ScalarBounded = requires(const T &value, const T &bound) {
    { value <= bound } -> std::convertible_to<bool>;
    { value >= bound } -> std::convertible_to<bool>;
};

template <glm::length_t L, typename T, glm::qualifier Q>
[[nodiscard]] constexpr auto componentwiseLessEqual(const glm::vec<L, T, Q> &lhs,
                                                    const glm::vec<L, T, Q> &rhs) -> bool
{
    return glm::all(glm::lessThanEqual(lhs, rhs));
}

template <glm::length_t L, typename T, glm::qualifier Q>
[[nodiscard]] constexpr auto componentwiseGreaterEqual(const glm::vec<L, T, Q> &lhs,
                                                       const glm::vec<L, T, Q> &rhs) -> bool
{
    return glm::all(glm::greaterThanEqual(lhs, rhs));
}

template <ScalarBounded T>
[[nodiscard]] constexpr auto componentwiseLessEqual(const T &lhs, const T &rhs) -> bool
{
    return lhs <= rhs;
}

template <ScalarBounded T>
[[nodiscard]] constexpr auto componentwiseGreaterEqual(const T &lhs, const T &rhs) -> bool
{
    return lhs >= rhs;
}

template <typename T>
concept ComponentwiseBounded = requires(const T &value, const T &bound) {
    { componentwiseLessEqual(value, bound) } -> std::same_as<bool>;
    { componentwiseGreaterEqual(value, bound) } -> std::same_as<bool>;
};

} // namespace detail

template <std::copyable T> class Parameter : public Property<T> {
  public:
    Parameter() = default;
    explicit Parameter(std::string name, T initialValue = {})
        : Property<T>(std::move(initialValue))
        , name_(std::move(name))
    {
    }

    [[nodiscard]] auto name() const noexcept -> std::string_view { return name_; }

    using Property<T>::changed;
    using Property<T>::get;
    using Property<T>::operator();
    using Property<T>::operator=;
    using Property<T>::set;

  private:
    std::string name_;
};

template <typename T>
    requires detail::ComponentwiseBounded<T>
class NumericParameter : public Parameter<T> {
  public:
    NumericParameter() = default;
    explicit NumericParameter(std::string name,
                              T initialValue = {},
                              std::optional<T> minValue = std::nullopt,
                              std::optional<T> maxValue = std::nullopt)
        : Parameter<T>(std::move(name), initialValue)
        , min_(std::move(minValue))
        , max_(std::move(maxValue))
    {
        CO_CORE_ASSERT(validateBounds(), "NumericParameter minimum must not exceed maximum");
        CO_CORE_ASSERT(validate(initialValue), "NumericParameter initial value is out of range");
    }

    [[nodiscard]] auto min() const noexcept -> const std::optional<T> &
    {
        return min_;
    }
    [[nodiscard]] auto max() const noexcept -> const std::optional<T> &
    {
        return max_;
    }
    [[nodiscard]] auto hasMin() const noexcept -> bool
    {
        return min_.has_value();
    }
    [[nodiscard]] auto hasMax() const noexcept -> bool
    {
        return max_.has_value();
    }

    [[nodiscard]] auto set(const T &value) -> bool
    {
        if (!validate(value)) {
            return false;
        }
        Property<T>::set(value);
        return true;
    }

    [[nodiscard]] auto operator=(const T &value) -> bool
    {
        return set(value);
    }

  private:
    [[nodiscard]] auto validateBounds() const -> bool
    {
        return !min_ || !max_ || detail::componentwiseLessEqual(*min_, *max_);
    }

    [[nodiscard]] auto validate(const T &value) const -> bool
    {
        if (min_ && !detail::componentwiseGreaterEqual(value, *min_)) {
            return false;
        }

        if (max_ && !detail::componentwiseLessEqual(value, *max_)) {
            return false;
        }

        return true;
    }

    std::optional<T> min_{};
    std::optional<T> max_{};
};

template <typename T, T... AllowedValues> class OptionParameter : public Parameter<T> {
  public:
    static_assert(sizeof...(AllowedValues) > 0, "OptionParameter requires at least one allowed value");

    static constexpr auto allowedValues = std::array<T, sizeof...(AllowedValues)>{AllowedValues...};

    OptionParameter() = default;
    explicit OptionParameter(std::string name, T initialValue = allowedValues.front())
        : Parameter<T>(std::move(name), initialValue)
    {
        CO_CORE_ASSERT(validate(initialValue), "OptionParameter initial value is not an accepted option");
    }

    [[nodiscard]] static constexpr auto acceptedValues() noexcept -> const auto &
    {
        return allowedValues;
    }

    [[nodiscard]] auto set(const T &value) -> bool
    {
        if (!validate(value)) {
            return false;
        }
        Property<T>::set(value);
        return true;
    }

    [[nodiscard]] auto operator=(const T &value) -> bool
    {
        return set(value);
    }

  private:
    [[nodiscard]] static auto validate(const T &value) -> bool
    {
        return std::ranges::contains(allowedValues, value);
    }
};

class StringOptionParameter : public Parameter<std::string> {
  public:
    StringOptionParameter() = default;

    explicit StringOptionParameter(std::string name, std::initializer_list<std::string> acceptedValues)
        : StringOptionParameter(std::move(name), std::vector<std::string>{acceptedValues})
    {
    }

    explicit StringOptionParameter(std::string name, std::vector<std::string> acceptedValues)
        : Parameter<std::string>(std::move(name),
                                 acceptedValues.empty() ? std::string{} : acceptedValues.front())
        , allowedValues_(std::move(acceptedValues))
    {
        CO_CORE_ASSERT(!allowedValues_.empty(),
                       "StringOptionParameter requires at least one accepted option");
    }

    explicit StringOptionParameter(std::string name,
                                   std::string initialValue,
                                   std::initializer_list<std::string> acceptedValues)
        : StringOptionParameter(std::move(name),
                                std::move(initialValue),
                                std::vector<std::string>{acceptedValues})
    {
    }

    explicit StringOptionParameter(std::string name,
                                   std::string initialValue,
                                   std::vector<std::string> acceptedValues)
        : Parameter<std::string>(std::move(name), std::move(initialValue))
        , allowedValues_(std::move(acceptedValues))
    {
        CO_CORE_ASSERT(!allowedValues_.empty(),
                       "StringOptionParameter requires at least one accepted option");
        CO_CORE_ASSERT(validate(this->get()),
                       "StringOptionParameter initial value is not an accepted option");
    }

    [[nodiscard]] auto acceptedValues() const noexcept -> std::span<const std::string>
    {
        return allowedValues_;
    }

    [[nodiscard]] auto set(const std::string &value) -> bool
    {
        if (!validate(value)) {
            return false;
        }
        Property<std::string>::set(value);
        return true;
    }

    [[nodiscard]] auto operator=(const std::string &value) -> bool
    {
        return set(value);
    }

  private:
    [[nodiscard]] auto validate(std::string_view value) const -> bool
    {
        return std::ranges::find(allowedValues_, value) != allowedValues_.end();
    }

    std::vector<std::string> allowedValues_{};
};

template <typename T>
    requires std::is_enum_v<T>
class EnumParameter : public Parameter<T> {
  public:
    static constexpr auto allowedValues = magic_enum::enum_values<T>();

    EnumParameter() = default;
    explicit EnumParameter(std::string name, T initialValue = allowedValues.front())
        : Parameter<T>(std::move(name), initialValue)
    {
        CO_CORE_ASSERT(validate(initialValue), "EnumParameter initial value is not a valid enum value");
    }

    [[nodiscard]] static constexpr auto acceptedValues() noexcept -> const auto &
    {
        return allowedValues;
    }

    [[nodiscard]] auto set(const T &value) -> bool
    {
        if (!validate(value)) {
            return false;
        }
        Property<T>::set(value);
        return true;
    }

    [[nodiscard]] auto operator=(const T &value) -> bool
    {
        return set(value);
    }

  private:
    [[nodiscard]] static auto validate(const T &value) -> bool
    {
        return magic_enum::enum_contains(value);
    }
};

} // namespace Cory
