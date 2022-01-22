#if ($member.is_const_ptr)const ${member.type}* ${member.name}{};
#else#if( $member.values )${member.type} ${member.name}{$member.values};#else${member.type} ${member.name};#end
#end