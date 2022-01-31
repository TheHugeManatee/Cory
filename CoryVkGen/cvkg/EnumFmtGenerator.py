from .SpecParser import SpecParser
from typing import List
from .GeneratorBase import GeneratorBase


class EnumFmtGenerator(GeneratorBase):
    def run(self, output_dir: str, registry: SpecParser, global_env: dict) -> List[str]:
        # these are for now ignored by hand, really need to figure out
        # how to actually parse the vulkan 1.2 feature set
        ignored_enums = ['API Constants', 'VkFullScreenExclusiveEXT', 'VkSwapchainImageUsageFlagBitsANDROID']
        search_list = [{'ignored_enums': ignored_enums}, global_env]

        return [self.write_template("FmtEnum.tpl.h", "FmtEnum.h", search_list)]
