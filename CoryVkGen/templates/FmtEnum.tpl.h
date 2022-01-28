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
} // namespace cvk

template <> struct fmt::formatter<VkResult> {
    constexpr auto parse(format_parse_context &ctx)  { return ctx.begin(); }

    template <typename FormatContext, typename VulkanEnumType>
    auto format(const VulkanEnumType &e, FormatContext &ctx)
    {
        return fmt::format_to(ctx.out(), cvk::to_string(e));
    }
};

// clang-format off
#for $enum in $enums
#if $enum.name != 'VkResult'
template <> struct fmt::formatter<$enum.name> : fmt::formatter<VkResult> {};
#end if
#end for
// clang-format on


#set bitmasks = [e for e in $registry.types.values() if e.category == 'bitmask' and e.requires != '']

namespace cvk {
template <typename VulkanFlagBitsType>
std::string flag_bits_to_string(VkFlags flagBits)
{
    auto curFlag = (VulkanFlagBitsType)1;
    if (!flagBits) return "( )";

    std::string flagBitsString("( ");
    while (curFlag) {
        if (flagBits & curFlag) {
            flagBitsString += std::string(to_string((VulkanFlagBitsType)curFlag)) + " ";
        }
        curFlag = (VulkanFlagBitsType)(curFlag << 1);
    }
    return flagBitsString + ")";
}

template <typename FlagBitsType, typename FlagsType> struct FlagBitsFormatter {
    constexpr auto parse(fmt::basic_format_parse_context<char> &ctx) { return ctx.begin(); }

    template <typename FormatContext, typename VulkanEnumType>
    auto format(const VulkanEnumType &e, FormatContext &ctx)
    {
        return format_to(ctx.out(), cvk::flag_bits_to_string<FlagBitsType, FlagsType>(e));
    }
};

}

#* Unfortunately this does not really work because all vulkan flags are just typedefs for VkFlags
// clang-format off
#for $bitmask in $bitmasks
template<> struct fmt::formatter<$bitmask.name> : cvk::FlagBitsFormatter<$bitmask.requires, $bitmask.name> {};
#end for
// clang-format on
*#
