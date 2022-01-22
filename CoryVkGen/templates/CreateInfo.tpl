struct ${create_info.name} {
#foreach ( $member in $create_info.members )
#parse("Member.tpl")
#end

};