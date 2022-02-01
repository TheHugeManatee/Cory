import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import List, Tuple, Dict
from .Types import *
import time
import copy


@dataclass
class FeatureLevel:
    types: Dict[str, Type]
    enums: Dict[str, Enum]
    commands: Dict[str, Command]


class SpecParser:
    types: Dict[str, Type]
    enums: Dict[str, Enum]
    commands: Dict[str, Command]
    feature_levels: Dict[str, FeatureLevel]
    silent: bool

    def __init__(self, vk_file_path, silent=False):
        print(f"Starting to parse '{vk_file_path}':")
        start_time = time.time()
        tree = ET.parse(vk_file_path)
        registry = tree.getroot()
        self.silent = silent

        self.parse_types(registry.find('types'))
        self.parse_commands(registry.find('commands'))
        self.parse_enums(registry.findall('enums'))
        self.parse_features(registry.findall('feature'))

        duration = time.time() - start_time

        if not silent:
            summary = self.feature_levels.copy()
            summary['REGISTRY'] = FeatureLevel(self.types, self.enums, self.commands)
            for k, v in summary.items():
                print(f"""===== {k} =====
    Types:     {len(v.types)}
    Commands:  {len(v.commands)}
    Enums:     {len(v.enums)}
    """)

        print(f"=== Parsed '{vk_file_path}' in {duration * 1000 : 3.1f} ms ===")

    def parse_types(self, types_n: ET.Element):
        if types_n is None:
            print('ERROR: <types> node not found on root node.')
            return

        types = [parse_type(type_n) for type_n in types_n.findall('type')]
        if not self.silent:
            print(f"{len(types)} Types found")

        self.types = {t.name: t for t in types if t}

        # some error/sanity checking
        unsuccessful = sum(c is None for c in types)
        if unsuccessful > 0:
            print(
                f"!!! {unsuccessful} <type> elements had errors while parsing!")
        unique_types = set([t.name for t in types if t])
        if len(unique_types) != len(types):
            names = [t.name for t in types if t]
            non_unique_types = set([n for n in names if names.count(n) > 1])
            print(
                f"!!! Detected {abs(len(unique_types) - len(types))} non-unique types: {non_unique_types}")

    def parse_commands(self, commands_n: ET.Element):
        if commands_n is None:
            print('ERROR: <commands> node not found on root node.')
            return

        commands = [parse_command(type_n)
                    for type_n in commands_n.findall('command')]
        if not self.silent:
            print(f"{len(commands)} commands found")

        self.commands = {c.name: c for c in commands}
        unsuccessful = sum(c is None for c in commands)
        if unsuccessful > 0:
            print(
                f"!!! {unsuccessful} <command> elements had errors while parsing!")

    def parse_enums(self, enums_ns: List[ET.Element]):
        if not enums_ns:
            print('ERROR: No <enums> nodes found on root node.')
            return

        if not self.silent:
            print(f"{len(enums_ns)} Enums found")

        enums = [parse_enum(en) for en in enums_ns]

        self.enums = {e.name: e for e in enums}
        unsuccessful = sum(c is None for c in enums)
        if unsuccessful > 0:
            print(
                f"!!! {unsuccessful} <enums> elements had errors while parsing!")

    def parse_features(self, features_ns: List[ET.Element]):
        if not features_ns:
            print('ERROR: No <feature> nodes found on root node.')
            return

        # seed the feature level with all basic types (stuff like 'uint32_t' etc.)
        basic_types = {n: t for n, t in self.types.items() if t.category == ''}
        feature_level = FeatureLevel(basic_types, {}, {})
        self.feature_levels = {}

        for feature_n in features_ns:
            if len(feature_n) == 0:
                # there's often empty ones, which we skip
                continue
            feature_level_name = feature_n.attrib['name']
            if not self.silent:
                print("Parsing feature level " + feature_level_name)

            for req_n in feature_n:
                if req_n.tag != 'require':
                    print(f"Found <{req_n.tag}> node: {req_n.attrib}")
                    continue
                # if 'comment' in req_n.attrib:
                #    print(f"  {req_n.attrib['comment']}")
                # else:
                #    print(f"Found unnamed <{req_n.tag}> node: {req_n.attrib}")

                self.aggregate_extension(req_n, feature_level)

            self.feature_levels[feature_level_name] = copy.deepcopy(feature_level)

    def aggregate_extension(self, node: ET.Element, fl: FeatureLevel):
        for n in node:
            if n.tag == 'comment': continue
            if 'name' not in n.attrib:
                print(f"Unnamed node: <{n.tag}> {n.attrib}")
                continue

            name = n.attrib['name']
            if n.tag == 'enum':
                if 'extends' in n.attrib:
                    extends = n.attrib['extends']
                    if extends not in self.enums:
                        print(f"Expected to extend unknown struct {extends}")
                    # further extend if possible, otherwise extend the base struct
                    struct_extends = self.enums[extends] if extends not in fl.enums else fl.enums[extends]
                    # skip adding if already in the list of values or if its an alias
                    if name in [ev.name for ev in struct_extends.values] or 'alias' in n.attrib:
                        continue

                    # append another enum value - there's probably a way to compute the 
                    # value from the `extnumber` and `offset` attributes but I don't really
                    # need the values here so I'm just putting None in there for now
                    comment = node.attrib['comment'] if 'comment' in node.attrib else "From Extension"
                    extended_values = struct_extends.values + [EnumValue(name, None, None, comment)]
                    fl.enums[extends] = Enum(struct_extends.name, struct_extends.type, extended_values)
                elif name in self.enums:
                    fl.enums[name] = self.enums[name]
                else:
                    pass
                    # these are just some API constants - we don't really care about those..
                    # print(f"   <{n.tag}> {name} UNKNOWN")
            elif n.tag == 'type':
                fl.types[name] = self.types[name]
            elif n.tag == 'command':
                fl.commands[name] = self.commands[name]
            else:
                print(f"No idea what to do with this node: <{n.tag}> {n.attrib}")

                # copy the remaining enums that have not been extended
            for type in fl.types.values():
                if type.category == 'enum' and type.name not in fl.enums:
                    fl.enums[type.name] = self.enums[type.name]
