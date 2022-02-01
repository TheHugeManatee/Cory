from .SpecParser import SpecParser
from typing import List
from .GeneratorBase import GeneratorBase
from dataclasses import dataclass
from .Types import *
from .GeneratorUtils import *


@dataclass
class Setter:
    param_name: str
    param_type: str
    setter_name: str
    set_to: str
    before_create: str
    builder_member: str


def clean_definition(definition: str, mname: str):
    return definition.replace(mname, '').strip()


@dataclass
class BuilderDefinition:
    create_info: Type
    create_cmd: Command
    created_handle: Type
    setters: List[Setter]
    builder_name: str
    par_list: List[str]

    def __init__(self, registry: SpecParser, create_info: str,
                 create_cmd: str, created_handle: str):
        self.create_info = registry.types[create_info]
        self.create_cmd = registry.commands[create_cmd]
        self.created_handle = registry.types[created_handle]
        self.builder_name = camel_to_snake(created_handle) + "_builder"
        self.setters = []
        self.par_list = []

        size_param, vector_param = self.find_vector_params(self.create_info.members)

        for s in self.create_info.members:
            if s.name == 'sType':
                continue
            # size parameter - we can skip that
            if s.name in size_param:
                continue

            # vector parameters are passed to the setter as a std::vector
            if s.name in vector_param:
                setter_name = camel_to_snake(s.name)
                param_type = f'std::vector<{s.type}>'
                set_to = f'{s.name}_ = std::move({s.name});'
                before_create = '\n'.join([f'        create_info_.{s.len} = static_cast<uint32_t>({s.name}_.size());',
                                           f'        create_info_.{s.name} = {s.name}_.data();'])
                builder_member = f'{param_type} {s.name}_;'
                self.setters.append(Setter(s.name, param_type, setter_name, set_to, before_create, builder_member))
            else:
                setter_name = camel_to_snake(s.name)
                param_type = clean_definition(s.definition, s.name)
                set_to = f'create_info_.{s.name} = {s.name};'
                self.setters.append(Setter(s.name, param_type, setter_name, set_to, '', ''))

        for p in self.create_cmd.params:
            if p.type == 'VkDevice':
                self.par_list.append('device_')
            elif p.name == 'pAllocator':
                self.par_list.append('nullptr')
            elif p.type == self.create_info.name:
                self.par_list.append('&create_info_')
            elif self.created_handle.name in p.definition:
                self.par_list.append('&created_thing')
            else:
                self.par_list.append(p.name)

    def find_vector_params(self, members: List[Member]):
        member_names = [m.name for m in members]
        arrays = {m.len: m.name for m in members if m.is_const_ptr and m.len in member_names}
        return arrays.keys(), arrays.values()


class BuilderGenerator(GeneratorBase):
    def run(self, output_dir: str, registry: SpecParser, global_env: dict) -> List[str]:
        builder_defs = [BuilderDefinition(registry, 'VkImageCreateInfo', 'vkCreateImage', 'VkImage'),
                        BuilderDefinition(registry, 'VkDeviceCreateInfo', 'vkCreateDevice', 'VkImage')
                        ]

        search_list = [{'builder_defs': builder_defs}, global_env]

        return [self.write_template("Builder.tpl.h", "Builder.h", search_list)]
