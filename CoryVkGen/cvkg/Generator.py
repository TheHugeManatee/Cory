from .SpecParser import SpecParser
from airspeed import Template,CachingFileLoader

import os
import os.path


class Generator:
    def __init__(self, registry: SpecParser, template_dir: str):
        self.registry = registry
        self.template_dir = template_dir

    def generate(self, output_folder: str):
        if not os.path.exists(output_folder):
            os.makedirs(output_folder)

        loader = CachingFileLoader(self.template_dir)

        create_infos = [c for c in self.registry.types.values() if not c.alias and "CreateInfo" in c.name]
        env = {
            'registry': self.registry,
            'create_infos': create_infos[:20]
        }

        for file in os.listdir(self.template_dir):
            # only generate direct outputs for templates for .cpp or .h files
            if file.endswith('.h') or file.endswith('.cpp'):
                out_file = os.path.join(output_folder, file.replace('.tpl', ''))
                template = loader.load_template(file)
                with open(out_file, 'w') as f:
                    instantiated_template = template.merge(env, loader)
                    f.write(instantiated_template)

                os.system(f'clang-format -i {out_file}')
                print(f"Generated {out_file}")