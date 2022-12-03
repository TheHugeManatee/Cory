#pragma once

#include <fmt/format.h>

#include <Corrade/Containers/StringView.h>
#include <magic_enum.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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

// formatters for glm vector types
template <typename glm::length_t L, typename ElementType>
struct fmt::formatter<glm::vec<L, ElementType>> : public fmt::formatter<ElementType> {
    auto format(glm::vec<L, ElementType> c, format_context &ctx)
    {
        auto out = ctx.out();
        *out = '(';
        ctx.advance_to(out);
        ctx.advance_to(formatter<ElementType>::format(c[0], ctx));
        *out = ',';
        ctx.advance_to(out);
        ctx.advance_to(formatter<ElementType>::format(c[1], ctx));
        if constexpr (L > 2) {
            *out = ',';
            ctx.advance_to(out);
            ctx.advance_to(formatter<ElementType>::format(c[2], ctx));
        }
        if constexpr (L > 3) {
            *out = ',';
            ctx.advance_to(out);
            ctx.advance_to(formatter<ElementType>::format(c[3], ctx));
        }

        *out = ')';
        ctx.advance_to(out);
        return out;
    }
};