#pragma once

#set enums = [e for e in $registry.enums.values() if e.name not in $ignored_enums]

\#include <fmt/format.h>
\#include <vulkan/vulkan.h>

namespace cvk {
#for $enum in $enums
constexpr std::string_view to_string($enum.name enum_value) noexcept
{
#if $enum.values
    switch (enum_value) {
#for $value in $enum.values
#if not $value.alias
    case $value.name:
        return "$value.name";
#end if
#end for
    }
    return "Unknown $enum.name value";
#else
    // no known enum values!
    return "Unknown";
#end if
};

#end for
}

template <> struct fmt::formatter<VkResult> {
    constexpr auto parse(format_parse_context &ctx)
    {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return ctx.end();
    }

    template <typename FormatContext, typename VulkanEnumType>
    auto format(const VulkanEnumType &e, FormatContext &ctx)
    {
        return format_to(ctx.out(), cvk::to_string(e));
    }
};

#for $enum in $enums
#if $enum.name != 'VkResult'
template <> struct fmt::formatter<$enum.name> : fmt::formatter<VkResult> {};
#end if
#end for

