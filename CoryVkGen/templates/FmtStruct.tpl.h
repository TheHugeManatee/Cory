#pragma once

\#include <fmt/format.h>
\#include <fmt/ranges.h>
\#include <vulkan/vulkan.h>

\#include "FmtEnum.h"

template <class ToStringFormattable> struct ToStringFormatter {
    constexpr auto parse(fmt::basic_format_parse_context<char> &ctx) { return ctx.begin(); }

    template <typename FormatContext, typename VulkanEnumType>
    auto format(const VulkanEnumType &e, FormatContext &ctx)
    {
        return format_to(ctx.out(), cvk::to_string(e));
    }
};

#for $struct in $structs + $unions
template <> struct fmt::formatter<$struct.name> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

    template <typename FormatContext, typename VulkanStruct>
    auto format(const VulkanStruct &s, FormatContext &ctx)
    {
        return fmt::format_to(ctx.out(), "${struct.name} {{\n"
#for $member in $struct.members
#if $member.name not in $ignored_members
            "  ${member.name} = {}\n"
#end if
#end for
        "}}"
#for $member in $struct.members
#if $member.name not in $ignored_members
#if ($member.is_const_ptr and not $member.definition.startswith('const char*')) or $member.name.startswith('pp') or $registry.types[$member.type].category in $void_cast_categories
        , (void*)s.${member.name}
#else
        , s.${member.name}
#end if
#end if
#end for
        );
    }
};
#end for
