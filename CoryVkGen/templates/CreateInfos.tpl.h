// This file was autogenerated from $current_file. DO NOT EDIT.

#pragma once

\#include <vulkan/vulkan.h>

namespace cvk
{

#for $create_info in $create_infos

struct ${create_info.name} {
#for $member in $create_info.members
#if $member.is_const_ptr
const ${member.type}* ${member.name}{};
#else
#if $member.values
${member.type} ${member.name}{$member.values};
#else
    ${member.type} ${member.name}{};
#end if
#end if
#end for
};

#end for

} // namespace cvk