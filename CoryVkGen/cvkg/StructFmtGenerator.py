from .SpecParser import SpecParser
from typing import List
import os.path
from Cheetah.Template import Template
from .GeneratorUtils import *


class StructFmtGenerator:
    def __init__(self, registry: SpecParser, template_dir: str):
        self.registry = registry
        self.template_dir = template_dir

    def run(self, output_dir: str, global_env: dict) -> List[str]:
        out_file = os.path.join(output_dir, "FmtStruct.h")
        template_path = os.path.join(self.template_dir, "FmtStruct.tpl.h")

        # these are for now ignored by hand, really need to figure out how to actually parse the vulkan 1.2 feature set
        ignored_structs = ['VkAndroidSurfaceCreateInfoKHR', 'VkViSurfaceCreateInfoNN', ]
        search_list = [global_env, {'current_file': out_file, 'ignored_structs': ignored_structs}]
        template = Template(file=template_path, searchList=search_list)

        with open(out_file, 'w') as f:
            f.write(str(template))

        return [out_file]
