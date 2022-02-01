from .SpecParser import SpecParser
from typing import List, Dict
import os.path
from Cheetah.Template import Template
from .GeneratorUtils import *


class GeneratorBase:
    def __init__(self, registry: SpecParser, template_dir: str, output_dir: str):
        self.registry = registry
        self.template_dir = template_dir
        self.output_dir = output_dir

    def write_template(self, template_file_base: str, out_file_base: str, search_list: List[Dict[str, str]]) -> str:
        out_file = os.path.join(self.output_dir, out_file_base)
        template_file = os.path.join(self.template_dir, template_file_base)

        # create parent if it does not exist
        if not os.path.exists(os.path.dirname(out_file)):
            os.makedirs(os.path.dirname(out_file))

        # enhance the context a bit
        sl = [{'current_file': out_file,
               'template_file': template_file
               }] + search_list

        # load template
        template = Template(file=template_file, searchList=sl)
        with open(out_file, 'w') as f:
            f.write(str(template))
        return out_file
