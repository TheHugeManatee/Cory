#pragma once

#include <Cory/Proper/Parameter.hpp>

#include <fmt/format.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

#include <KDGpu/utils/flags.h>

#include <concepts>
#include <string>
#include <string_view>

namespace CoImGui {

namespace detail {
static constexpr float LABEL_WIDTH{150.0f};

inline float availableWidth()
{
    return ::ImGui::GetContentRegionAvail().x;
}

inline void Label(std::string_view label)
{
    const auto labelText = std::string{label};
    ::ImGui::TextUnformatted(labelText.c_str());
}

template <typename T>
concept MutableValueHolder = requires(T holder, const T constHolder) {
    constHolder.get();
    holder.set(constHolder.get());
};

template <typename T>
concept NamedValueHolder = MutableValueHolder<T> && requires(const T constHolder) {
    { constHolder.name() } -> std::convertible_to<std::string_view>;
};

template <typename T>
concept StringOptionsHolder = NamedValueHolder<T> && requires(const T constHolder) {
    requires std::same_as<std::remove_cvref_t<decltype(constHolder.get())>, std::string>;
    constHolder.acceptedValues();
};

template <typename Holder, typename Value>
auto setValue(Holder &holder, Value &&value) -> bool
{
    if constexpr (std::same_as<decltype(holder.set(std::forward<Value>(value))), bool>) {
        return holder.set(std::forward<Value>(value));
    }
    else {
        holder.set(std::forward<Value>(value));
        return true;
    }
}

template <typename T>
[[nodiscard]] constexpr auto sliderBoundValue(const T &value) -> const T &
{
    return value;
}

template <glm::length_t L, typename T, glm::qualifier Q>
[[nodiscard]] constexpr auto sliderBoundValue(const glm::vec<L, T, Q> &value) -> T
{
    return value.x;
}

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
    detail::Label(label);
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
    detail::Label(label);
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

template <detail::MutableValueHolder Holder, typename... Arguments>
auto Slider(std::string_view label, Holder &holder, Arguments... args)
{
    auto v = holder.get();
    if (Slider(label, v, args...)) {
        return detail::setValue(holder, v);
    }
    return false;
}

template <detail::NamedValueHolder Holder, typename... Arguments>
auto Slider(Holder &holder, Arguments... args)
{
    return Slider(holder.name(), holder, args...);
}

template <typename T>
auto Slider(Cory::NumericParameter<T> &parameter)
{
    if (!parameter.hasMin() || !parameter.hasMax()) {
        return false;
    }

    return Slider(parameter.name(),
                  parameter,
                  detail::sliderBoundValue(*parameter.min()),
                  detail::sliderBoundValue(*parameter.max()));
}

// template for double, float, int
template <typename ValueType, typename... Arguments>
    requires std::same_as<ValueType, double> || std::same_as<ValueType, float> ||
             std::same_as<ValueType, int32_t>
auto Input(std::string_view label, ValueType &value, Arguments... args)
{
    detail::Label(label);
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
    detail::Label(label);
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

template <detail::MutableValueHolder Holder, typename... Arguments>
auto Input(std::string_view label, Holder &holder, Arguments... args)
{
    auto v = holder.get();
    if (Input(label, v, args...)) {
        return detail::setValue(holder, v);
    }
    return false;
}

template <detail::NamedValueHolder Holder, typename... Arguments>
auto Input(Holder &holder, Arguments... args)
{
    return Input(holder.name(), holder, args...);
}

template <typename E>
    requires std::is_enum_v<E>
bool ComboBox(std::string_view label, E &value, ImGuiComboFlags flags = 0)
{
    detail::Label(label);
    ::ImGui::SameLine(detail::availableWidth() / 3.0f);

    // Pass in the preview value visible before opening the combo (it could technically be different
    // contents or not pulled from items[])
    auto valueName = magic_enum::enum_name(value);
    bool wasChanged = false;
    if (ImGui::BeginCombo(label.data(), valueName.data(), flags)) {
        for (auto v : magic_enum::enum_values<E>()) {
            auto vName = magic_enum::enum_name(v);
            const bool is_selected = value == v;
            if (ImGui::Selectable(vName.data(), is_selected)) {
                value = v;
                wasChanged = true;
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return wasChanged;
}

template <detail::MutableValueHolder Holder>
    requires std::is_enum_v<std::remove_cvref_t<decltype(std::declval<const Holder &>().get())>>
bool ComboBox(std::string_view label, Holder &holder)
{
    auto v = holder.get();
    if (ComboBox(label, v)) {
        return detail::setValue(holder, v);
    }
    return false;
}

template <detail::NamedValueHolder Holder>
    requires std::is_enum_v<std::remove_cvref_t<decltype(std::declval<const Holder &>().get())>>
bool ComboBox(Holder &holder)
{
    return ComboBox(holder.name(), holder);
}

template <detail::StringOptionsHolder Holder>
bool ComboBox(std::string_view label, Holder &holder, ImGuiComboFlags flags = 0)
{
    detail::Label(label);
    ::ImGui::SameLine(detail::availableWidth() / 3.0f);

    const auto currentValue = holder.get();
    bool wasChanged = false;
    if (ImGui::BeginCombo(label.data(), currentValue.c_str(), flags)) {
        for (const auto &option : holder.acceptedValues()) {
            const bool isSelected = currentValue == option;
            if (ImGui::Selectable(option.c_str(), isSelected)) {
                wasChanged = detail::setValue(holder, option);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return wasChanged;
}

template <detail::StringOptionsHolder Holder>
bool ComboBox(Holder &holder, ImGuiComboFlags flags = 0)
{
    return ComboBox(holder.name(), holder, flags);
}

template <typename E>
    requires std::is_enum_v<E>
bool ComboBox(std::string_view label, KDGpu::Flags<E> &flags)
{
    bool wasChanged = false;
    E v{flags.toInt()};
    if (ComboBox(label, v)) {
        flags = KDGpu::Flags<E>(v);
        wasChanged = true;
    }
    return wasChanged;
}

template <detail::MutableValueHolder Holder>
    requires std::same_as<std::remove_cvref_t<decltype(std::declval<const Holder &>().get())>, bool>
inline bool CheckBox(const char *str, Holder &holder)
{
    bool v = holder.get();
    if (ImGui::Checkbox(str, &v)) {
        return detail::setValue(holder, v);
    }
    return false;
}

template <detail::NamedValueHolder Holder>
    requires std::same_as<std::remove_cvref_t<decltype(std::declval<const Holder &>().get())>, bool>
inline bool CheckBox(Holder &holder)
{
    const auto label = std::string{holder.name()};
    return CheckBox(label.c_str(), holder);
}

template <typename E>
    requires std::is_enum_v<E>
bool CheckBoxFlags(std::string_view label, KDGpu::Flags<E> &flags)
{
    Text("{}", label);
    ImGui::BeginGroup();
    ImGui::Indent();

    bool wasChanged = false;
    for (auto flag : magic_enum::enum_values<E>()) {
        bool flagSet = flags.testFlag(flag);
        if (ImGui::Checkbox(fmt::format("{}##{}", magic_enum::enum_name(flag), label).c_str(),
                            &flagSet)) {
            if (flagSet) {
                flags |= flag;
            }
            else {
                // Gpu::Flags does not have a clearFlag, and doesn't implement the ~ operator :(
                flags &= E{~std::to_underlying(flag)};
            }
            wasChanged = true;
        }
    }
    ImGui::Unindent();
    ImGui::EndGroup();
    return wasChanged;
}

} // namespace CoImGui
