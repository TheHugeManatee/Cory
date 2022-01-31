#pragma once
\#include <vulkan/vulkan.h>
\#include <stdexcept>

namespace cvk {

    class vulkan_error : public std::runtime_error {
      public:
        vulkan_error(VkResult result) : std::runtime_error(fmt::format("Vulkan error: {}", result)) {}
    };

    void raise_if_error(VkResult result) {
        if(result != VK_SUCCESS) {
            throw vulkan_error(result);
        }
    }

#for $bd in $builder_defs
#set ci = $bd.create_info
#set built_cls = $bd.created_handle.name

class $bd.builder_name {
  public:
    ${bd.builder_name}(VkDevice device) : device_{device} { }

#for $s in $bd.setters
        ${bd.builder_name}& ${s.setter_name}($s.param_type $s.param_name) {
            $s.set_to
            return *this;
        }
#end for

    [[nodiscard]] $built_cls create() {
        create_info_.sType = $ci.members[0].values;
        $bd.created_handle.name created_thing;

#for $s in $bd.setters
#if s.before_create
$s.before_create
#end if
#end for

        VkResult result = ${bd.create_cmd.name}(#slurp
#echo ', '.join($bd.par_list)
);
        raise_if_error(result);
        return created_thing;
    }
  private:
    VkDevice device_;
#for $s in $bd.setters
#if s.builder_member
    $s.builder_member
#end if
#end for
    $ci.name create_info_{};
};



#end for

}