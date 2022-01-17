import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import List,Tuple
from .Types import *
import time

class SpecParser:
    def __init__(self, vk_file_path):
        print(f"Starting to parse '{vk_file_path}':")
        start_time = time.time()
        tree = ET.parse(vk_file_path)
        registry = tree.getroot()
        
        self.parse_types(registry.find('types'))
        self.parse_commands(registry.find('commands'))
        self.parse_enums(registry.findall('enums'))
        

        duration = time.time() - start_time

        print(f"""===== Summary =====
Types:     {len(self.types)}
Commands:  {len(self.commands)}
Enums:     {len(self.enums)}
""")

        print(f"=== Parsed '{vk_file_path}' in {duration*1000 : 3.1f} ms ===")

    def parse_types(self, types_n: ET.Element):
        if types_n is None:
            print('ERROR: <types> node not found on root node.')
            return

        types = [parse_type(type_n) for type_n in types_n.findall('type')]
        print(f"{len(types)} Types found")

        self.types = {t.name: t for t in types if t}

        # some error/sanity checking
        unsuccessful = sum(c == None for c in types)
        if unsuccessful > 0:
            print(f"!!! {unsuccessful} <type> elements had errors while parsing!")
        unique_types = set([t.name for t in types if t])
        if len(unique_types) != len(types):
            names = [t.name for t in types if t]
            non_unique_types = set([n for n in names if names.count(n) > 1])
            print(f"!!! Detected {abs(len(unique_types) - len(types))} non-unique types: {non_unique_types}")

        # list a summary of the categories
        # cats = {}
        # for type in self.types.values():
        #     cat = type.category if type.category != '' else '<no cat>'
        #     if cat not in cats:
        #         cats[cat] = 1
        #     else:
        #         cats[cat] += 1

        # for c,v in cats.items():
        #     print(f"  - {c : <14}{v}")
        # print('')

    def parse_commands(self, commands_n: ET.Element):
        if commands_n is None:
            print('ERROR: <commands> node not found on root node.')
            return

        commands = [parse_command(type_n) for type_n in commands_n.findall('command')]
        print(f"{len(commands)} commands found")

        self.commands = {c.name : c for c in commands}
        unsuccessful = sum(c == None for c in commands)
        if unsuccessful > 0:
            print(f"!!! {unsuccessful} <command> elements had errors while parsing!")
        

    def parse_enums(self, enums_ns: List[ET.Element]):
        if not enums_ns:
            print('ERROR: No <enums> nodes found on root node.')
            return

        print(f"{len(enums_ns)} Enums found")

        enums = [parse_enum(en) for en in enums_ns]

        self.enums = {e.name : e for e in enums}
        unsuccessful = sum(c == None for c in enums)
        if unsuccessful > 0:
            print(f"!!! {unsuccessful} <enums> elements had errors while parsing!")