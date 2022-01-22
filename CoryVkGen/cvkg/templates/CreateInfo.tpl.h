struct ${create_info.name} {
#foreach ( $member in $create_info.members )
    #if( $member.values )${member.type} ${member.name}{$member.values};#else${member.type} ${member.name};#end
#end

};