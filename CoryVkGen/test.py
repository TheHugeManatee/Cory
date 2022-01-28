from cvkg import SpecParser
from cvkg.Types import *

from typing import List,Tuple

def undefined_enums(registry):
    # alias enums are ok and there are some enums that are simply
    # not further defined in the spec file i'm looking at?!
    ignored_enums = ['VkQueryPoolCreateFlagBits', 'VkInstanceCreateFlagBits','VkDeviceCreateFlagBits']
    def is_relevant(t) -> bool:
        return t.name not in registry.enums.keys() \
                and t.category == 'enum' and not t.alias and not t.name in ignored_enums
    return [t for t in registry.types.values() if is_relevant(t)]

def has_unresolved_types(registry, paramsOrMembers: List[Tuple[Parameter,Member]])->List[str]:
    unresolved = []
    for p in paramsOrMembers:
        if p.type not in registry.types:
            unresolved.append(p.type)
    return unresolved

def types_with_undefined_members(registry):
    return [t for t in registry.types.values() if len(has_unresolved_types(registry, t.members)) > 0]

def commands_with_undefined_parameters(registry):
    return [c for c in registry.commands.values() if not c.alias and len(has_unresolved_types(registry, c.params)) > 0]

if __name__== '__main__':
    registry = SpecParser("c:/tmp/vk.v1.2.203.xml")

    feature_set = registry.feature_levels['VK_VERSION_1_0']
    # list enum type definitions that don't have any actual enum value listings
    ue = undefined_enums(feature_set)
    if len(ue) > 0:
        print(f"Found {len(ue)} undefined enums:")
        for e in ue:
            print(e)

    # list types with undefined member types
    twum = types_with_undefined_members(feature_set)
    if len(twum) > 0:
        print("Found types with undefined member types: ")
        for t in twum:
            print(t)

    # list commands with undefined parameter types
    cwup = commands_with_undefined_parameters(feature_set)
    if len(cwup) > 0:
        print("Found commands with undefined parameter types: ")
        for c in cwup:
            print(c)



