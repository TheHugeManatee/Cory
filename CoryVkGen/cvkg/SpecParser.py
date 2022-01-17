import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import List,Tuple
from .Types import *



class SpecParser:
    def __init__(self, vk_file_path):
        print(f"Starting to parse '{vk_file_path}':")

        tree = ET.parse(vk_file_path)
        registry = tree.getroot()

        print("root tag is '{}'".format(registry.tag))
        
        self.parse_types(registry.find('types'))
        self.parse_commands(registry.find('commands'))


    def parse_types(self, types_n: ET.Element):
        if types_n is None:
            print('ERROR: <types> node not found on root node.')
            return

        self.types = [parse_type(type_n) for type_n in types_n.findall('type')]
        unsuccessful = sum(c == None for c in self.types)
        if unsuccessful > 0:
            print(f"!!! {unsuccessful} <type> elements had errors while parsing!")


        cats = {}
        print(f"=== {len(self.types)} Types parsed ===")
        for type in self.types:
            cat = type.category if type.category is not None else '<no cat>'
            if cat not in cats:
                cats[cat] = 1
            else:
                cats[cat] += 1

        for c,v in cats.items():
            print(f"  - {c : <14}{v}")
        print('')

    def parse_commands(self, commands_n: ET.Element):
        if commands_n is None:
            print('ERROR: <commands> node not found on root node.')
            return

        self.commands = [parse_command(type_n) for type_n in commands_n.findall('command')]
        print(f"=== {len(self.commands)} Commands parsed ===")
        unsuccessful = sum(c == None for c in self.commands)
        if unsuccessful > 0:
            print(f"!!! {unsuccessful} <command> elements had errors while parsing!")
        