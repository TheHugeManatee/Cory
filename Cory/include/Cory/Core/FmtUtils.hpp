#pragma once

#include <fmt/format.h>

#include <Corrade/Containers/StringView.h>
#include <magic_enum.hpp>

#if !defined(MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT)
#define MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT true
#define MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT_AUTO_DEFINE
#endif // MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT

namespace magic_enum::customize {
// customize enum to enable/disable automatic std::format
template <typename E> constexpr bool enum_format_enabled() noexcept
{
    return MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT;
}
} // namespace magic_enum::customize

#include <format>

template <typename E>
struct fmt::formatter<
    E,
    std::enable_if_t<std::is_enum_v<E> && magic_enum::customize::enum_format_enabled<E>(), char>>
    : fmt::formatter<std::string_view, char> {
    auto format(E e, format_context &ctx) const
    {
        using D = std::decay_t<E>;
        if constexpr (magic_enum::detail::is_flags_v<D>) {
            if (auto name = magic_enum::enum_flags_name<D>(e); !name.empty()) {
                return this->fmt::formatter<std::string_view, char>::format(
                    std::string_view{name.data(), name.size()}, ctx);
            }
        }
        else {
            if (auto name = magic_enum::enum_name<D>(e); !name.empty()) {
                return this->fmt::formatter<std::string_view, char>::format(
                    std::string_view{name.data(), name.size()}, ctx);
            }
        }
        // fallback - format as an integer + hex value
        return fmt::format_to(ctx.out(),
                              "{} (0x{:X})",
                              magic_enum::enum_integer<D>(e),
                              magic_enum::enum_integer<D>(e));
    }
};

#if defined(MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT_AUTO_DEFINE)
#undef MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT
#undef MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT_AUTO_DEFINE
#endif // MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT_AUTO_DEFINE

// formatter for Corrade::Containers::StringView
template <> struct fmt::formatter<Corrade::Containers::StringView, char> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }
    auto format(Corrade::Containers::StringView v, format_context &ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", std::string_view{v.data(), v.size()});
    }
};