#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>
#include <kdbindings/property.h>

#include <concepts>
#include <fmt/format.h>
#include <string>
#include <string_view>

namespace CoImGui {

namespace detail {
static constexpr float LABEL_WIDTH{150.0f};

inline float availableWidth() { return ::ImGui::GetContentRegionAvail().x; }

} // namespace detail

template <typename... Args> void Text(fmt::format_string<Args...> fmtString, Args... args)
{
    std::string formatted = fmt::format(fmtString, std::forward<Args>(args)...);
    ::ImGui::Text("%s", formatted.c_str()); // NOLINT
}

// template for float, int
template <typename ValueType, typename... Arguments>
    requires std::same_as<ValueType, float> || std::same_as<ValueType, int32_t>
auto Slider(std::string_view label, ValueType &value, Arguments... args)
{
    ::ImGui::Text("%s", label.data()); // NOLINT
    ::ImGui::SameLine(detail::availableWidth() / 3.0f);
    const std::string internalLabel = fmt::format("##{}", label);
    if constexpr (std::same_as<ValueType, float>) {
        return ::ImGui::SliderFloat(internalLabel.c_str(), &value, args...);
    }
    if constexpr (std::same_as<ValueType, int32_t>) {
        return ::ImGui::SliderInt(internalLabel.c_str(), &value, args...);
    }
}

// template for glm::vec<L, T>
template <glm::length_t L, typename T, typename... Arguments>
auto Slider(std::string_view label, glm::vec<L, T> &value, Arguments... args)
{
    ::ImGui::Text("%s", label.data()); // NOLINT
    ::ImGui::SameLine(detail::availableWidth() / 3.0f);
    const std::string internalLabel = fmt::format("##{}", label);
    if constexpr (L == 2) {
        if constexpr (std::same_as<T, float>) {
            return ::ImGui::SliderFloat2(internalLabel.c_str(), &value.x, args...);
        }
        if constexpr (std::same_as<T, int32_t>) {
            return ::ImGui::SliderInt(internalLabel.c_str(), &value.x, args...);
        }
    }
    if constexpr (L == 3) {
        if constexpr (std::same_as<T, float>) {
            return ::ImGui::SliderFloat3(internalLabel.c_str(), &value.x, args...);
        }
        if constexpr (std::same_as<T, int32_t>) {
            return ::ImGui::SliderInt3(internalLabel.c_str(), &value.x, args...);
        }
    }
    if constexpr (L == 4) {
        if constexpr (std::same_as<T, float>) {
            return ::ImGui::SliderFloat4(internalLabel.c_str(), &value.x, args...);
        }
        if constexpr (std::same_as<T, int32_t>) {
            return ::ImGui::SliderInt4(internalLabel.c_str(), &value.x, args...);
        }
    }
}

// template for KDBindings::Property
template <typename Property, typename... Arguments>
    requires KDBindings::Private::is_property<Property>::value
auto Slider(std::string_view label, Property &property, Arguments... args)
{
    auto v = property.get();
    if (Slider(label, v, args...)) {
        property.set(v);
        return true;
    }
    return false;
}

// template for double, float, int
template <typename ValueType, typename... Arguments>
    requires std::same_as<ValueType, double> || std::same_as<ValueType, float> ||
             std::same_as<ValueType, int32_t>
auto Input(std::string_view label, ValueType &value, Arguments... args)
{
    ::ImGui::Text("%s", label.data()); // NOLINT
    ::ImGui::SameLine(detail::availableWidth() / 3.0f);
    const std::string internalLabel = fmt::format("##{}", label);
    if constexpr (std::same_as<ValueType, double>) {
        return ::ImGui::InputDouble(internalLabel.c_str(), &value, args...);
    }
    if constexpr (std::same_as<ValueType, float>) {
        return ::ImGui::InputFloat(internalLabel.c_str(), &value, args...);
    }
    if constexpr (std::same_as<ValueType, int32_t>) {
        return ::ImGui::InputInt(internalLabel.c_str(), &value, args...);
    }
}

// template for glm::vec<L, T>
template <glm::length_t L, typename T, typename... Arguments>
auto Input(std::string_view label, glm::vec<L, T> &value, Arguments... args)
{
    ::ImGui::Text("%s", label.data()); // NOLINT
    ::ImGui::SameLine(detail::availableWidth() / 3.0f);
    const std::string internalLabel = fmt::format("##{}", label);
    if constexpr (L == 2) {
        if constexpr (std::same_as<T, float>) {
            return ::ImGui::InputFloat2(internalLabel.c_str(), &value.x, args...);
        }
        if constexpr (std::same_as<T, int32_t>) {
            return ::ImGui::InputInt(internalLabel.c_str(), &value.x, args...);
        }
    }
    if constexpr (L == 3) {
        if constexpr (std::same_as<T, float>) {
            return ::ImGui::InputFloat3(internalLabel.c_str(), &value.x, args...);
        }
        if constexpr (std::same_as<T, int32_t>) {
            return ::ImGui::InputInt3(internalLabel.c_str(), &value.x, args...);
        }
    }
    if constexpr (L == 4) {
        if constexpr (std::same_as<T, float>) {
            return ::ImGui::InputFloat4(internalLabel.c_str(), &value.x, args...);
        }
        if constexpr (std::same_as<T, int32_t>) {
            return ::ImGui::InputInt4(internalLabel.c_str(), &value.x, args...);
        }
    }
}

// template for KDBindings::Property
template <typename Property, typename... Arguments>
    requires KDBindings::Private::is_property<Property>::value
auto Input(std::string_view label, Property &property, Arguments... args)
{
    auto v = property.get();
    if (Input(label, v, args...)) {
        property.set(v);
        return true;
    }
    return false;
}

} // namespace CoImGui