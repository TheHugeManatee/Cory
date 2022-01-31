import re

def split_uppercase(s):
    return [a for a in re.split(r'([A-Z][a-z]*)', s) if a]

def structure_type_name(type_name:str):
    parts = split_uppercase(type_name)[1:]
    stype_parts = ['vk','structure','type']+parts
    return "_".join([s.upper() for s in stype_parts])

def camel_to_snake(s:str, remove_prefixes=True):
    parts = split_uppercase(s)
    if remove_prefixes:
        while parts[0] in ['p', 'pp','vk','Vk']:
            parts = parts[1:]
    # merge extension suffix
    start_suffix_merge = len(parts)
    while len(parts[start_suffix_merge-1]) == 1:
        start_suffix_merge -= 1
    if start_suffix_merge < len(parts):
        parts[start_suffix_merge:] = [''.join(parts[start_suffix_merge:])]
    return "_".join([s.lower() for s in parts])

