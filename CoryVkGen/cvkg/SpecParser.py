import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import List,Tuple

@dataclass(frozen=True)
class Member:
    name: str
    type: str
    values: str
    is_optional: bool
    is_const_ref: bool
    definition: str
    len: str


@dataclass(frozen=True)
class Type:
    name: str
    category: str
    is_typedef: bool
    alias: str



class SpecParser:
    def __init__(self, vk_file_path):
        print(f"Starting to parse '{vk_file_path}':")

        tree = ET.parse(vk_file_path)
        registry = tree.getroot()

        print("root tag is '{}'".format(registry.tag))
        for child in registry:
            print(f'<{child.tag}>: {child.attrib}')
        