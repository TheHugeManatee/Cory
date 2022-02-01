from .SpecParser import SpecParser
from typing import List
from .GeneratorBase import GeneratorBase


class StructFmtGenerator(GeneratorBase):
    def run(self, output_dir: str, registry: SpecParser, global_env: dict) -> List[str]:
        # these are for now ignored by hand, really need to figure out
        # how to actually parse the vulkan 1.2 feature set
        ignored_structs = ['VkAndroidSurfaceCreateInfoKHR', 'VkViSurfaceCreateInfoNN', ]
        unions = [s for s in registry.types.values() if s.category == 'union']
        structs = [s for s in registry.types.values() if
                   s.name not in ignored_structs and s.category == 'struct' and not s.alias]
        ignored_members = ['sType', 'pNext']
        void_cast_categories = ['handle', 'funcpointer']
        inlined_structs = ['VkExtent2D', 'VkExtent3D', 'VkOffset2D', 'VkOffset3D']

        search_list = [locals(), global_env]

        return [
            self.write_template("FmtStruct.tpl.h", "include/cvk/FmtStruct.h", search_list)
        ]
