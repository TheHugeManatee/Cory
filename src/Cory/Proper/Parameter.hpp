#pragma once

#include "Property.hpp"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace Cory::Proper {

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

template <typename T, T Min, T Max>
    requires std::totally_ordered<T>
class RangedParameter : public Parameter<T> {
  public:
    static_assert(Min <= Max, "RangedParameter minimum must not exceed maximum");

    RangedParameter() = default;
    explicit RangedParameter(std::string name, T initialValue = Min)
        : Parameter<T>(std::move(name), initialValue)
    {
        validate(initialValue);
    }

    [[nodiscard]] static constexpr auto min() noexcept -> T { return Min; }
    [[nodiscard]] static constexpr auto max() noexcept -> T { return Max; }

    auto set(const T &value) -> void
    {
        validate(value);
        Property<T>::set(value);
    }

    auto operator=(const T &value) -> RangedParameter &
    {
        set(value);
        return *this;
    }

  private:
    static auto validate(const T &value) -> void
    {
        if (value < Min || value > Max) {
            throw std::out_of_range("Parameter value out of range");
        }
    }
};

template <typename T, T... AllowedValues> class OptionParameter : public Parameter<T> {
  public:
    static_assert(sizeof...(AllowedValues) > 0, "OptionParameter requires at least one allowed value");

    static constexpr auto allowedValues = std::array<T, sizeof...(AllowedValues)>{AllowedValues...};

    OptionParameter() = default;
    explicit OptionParameter(std::string name, T initialValue = allowedValues.front())
        : Parameter<T>(std::move(name), initialValue)
    {
        validate(initialValue);
    }

    [[nodiscard]] static constexpr auto acceptedValues() noexcept -> const auto &
    {
        return allowedValues;
    }

    auto set(const T &value) -> void
    {
        validate(value);
        Property<T>::set(value);
    }

    auto operator=(const T &value) -> OptionParameter &
    {
        set(value);
        return *this;
    }

  private:
    static auto validate(const T &value) -> void
    {
        if (!std::ranges::contains(allowedValues, value)) {
            throw std::invalid_argument("Parameter value is not an accepted option");
        }
    }
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
        validate(initialValue);
    }

    [[nodiscard]] static constexpr auto acceptedValues() noexcept -> const auto &
    {
        return allowedValues;
    }

    auto set(const T &value) -> void
    {
        validate(value);
        Property<T>::set(value);
    }

    auto operator=(const T &value) -> EnumParameter &
    {
        set(value);
        return *this;
    }

  private:
    static auto validate(const T &value) -> void
    {
        if (!magic_enum::enum_contains(value)) {
            throw std::invalid_argument("Parameter value is not a valid enum value");
        }
    }
};

} // namespace Cory::Proper
