import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import List, Tuple
from .flextglutils import *
import re


def str_attrib(node: ET.Element, attrib: str) -> str:
    if attrib in node.attrib:
        return node.attrib[attrib]
    return ''


def list_attrib(node: ET.Element, attrib: str) -> List[str]:
    if attrib in node.attrib:
        return node.attrib[attrib].split(',')
    return []


def bool_attrib(node: ET.Element, attrib: str) -> bool:
    if attrib in node.attrib:
        a = node.attrib[attrib]
        if a == 'true':
            return True
        elif a == 'false':
            return False
        else:
            #raise Exception(f"I don't know how to parse '{a}' as a bool")
            # some have have optional='true,false', no idea what that is supposed to mean...
            return None
    return False


def subnode_text(node: ET.Element, subnode_tag: str, direct_child=False) -> str:
    subnodes = node.findall(
        subnode_tag) if not direct_child else node.find(subnode_tag)
    if len(subnodes) == 1:
        return subnodes[0].text
    return ''


def is_cptr_definition(definition: str) -> bool:
    return re.match("const (.+)\* (.+)", definition) != None


@dataclass(frozen=True)
class Member:
    name: str
    type: str
    values: str
    is_optional: bool
    is_const_ptr: bool
    definition: str

    def __repr__(self):
        type = f"const {self.type}*" if self.is_const_ptr else self.type
        return f"{'?' if self.is_optional else ' '}{self.name}: {type}"


def parse_member(node: ET.Element) -> Member:
    definition = extract_text(node)
    return Member(
        name=node.find('name').text,
        type=node.find('type').text,
        values=str_attrib(node, 'values'),
        is_optional=bool_attrib(node, 'optional'),
        is_const_ptr=is_cptr_definition(definition),
        definition=definition
    )


@dataclass(frozen=True)
class Type:
    name: str
    category: str
    alias: str
    members: List[Member]
    definition: str
    requires: str

    def __repr__(self):
        if self.alias:
            return f'[{self.category}] {self.name} --> {self.alias}'

        members = '\n'.join([f'[{self.category}] {self.name}'] + [f'  - {p}' for p in self.members])
        return members.replace('\n', '\n  ')


def parse_type(node: ET.Element) -> Type:
    definition = extract_text(node)
    category = str_attrib(node, 'category')
    name = str_attrib(node, 'name')

    # some of them store the name as a <name>...</name> sub-element instead
    if category in ['define', 'basetype', 'handle', 'bitmask', 'funcpointer']:
        name_n = node.findall('name')
        if len(name_n) == 1:
            name = name_n[0].text

    if name == '':
        print(
            f"Error parsing <{node.tag}>: {node.attrib} -- {extract_text(node)}")
        return None

    return Type(
        name=name,
        category=category,
        # for basetype or bitmask types, the typedef might be in a <type> subnode
        alias=subnode_text(node, 'type') if category in [
            'basetype', 'bitmask'] else str_attrib(node, 'alias'),
        members=[parse_member(member_n)
                 for member_n in node.findall('member')],
        definition=definition,
        requires=str_attrib(node, 'requires')
    )


@dataclass(frozen=True)
class Parameter:
    name: str
    type: str
    values: str
    is_optional: bool
    is_const_ptr: bool
    definition: str

    def __repr__(self):
        type = f"const {self.type}*" if self.is_const_ptr else self.type
        return f"{'?' if self.is_optional else ' '}{self.name}: {type}"


def parse_parameter(node: ET.Element) -> Parameter:
    definition = extract_text(node)
    return Parameter(
        name=node.find('name').text,
        type=node.find('type').text,
        values=str_attrib(node, 'values'),
        is_optional=bool_attrib(node, 'optional'),
        is_const_ptr=is_cptr_definition(definition),
        definition=definition
    )


@dataclass(frozen=True)
class Command:
    name: str
    returnType: str
    params: List[Parameter]
    alias: str
    queues: List[str]
    renderpass: str
    successcodes: List[str]
    errorcodes: List[str]
    cmdbufferlevel: List[str]

    def __repr__(self):
        if self.alias:
            return f'[command] {self.name}(...) --> {self.alias}'

        params = '\n'.join(f'  - {p}' for p in self.params)
        params = params.replace('\n', '\n  ')
        return f'[command] {self.name}(...) -> {self.returnType}\n{params}'


def parse_command(node: ET.Element) -> Command:
    proto_n = node.find('proto')
    if not proto_n:
        if 'alias' in node.attrib:
            return Command(name=str_attrib(node, 'name'), returnType=None, params=None,
                           alias=str_attrib(node, 'alias'), queues=None, renderpass=None,
                           successcodes=None, errorcodes=None, cmdbufferlevel=None)
        else:
            print(f'Error parsing <{node.tag}>: {node.attrib}')
            return None
    return Command(
        name=proto_n.find('name').text,
        returnType=proto_n.find('type').text,
        params=[parse_parameter(pn) for pn in node.findall('param')],
        alias=node.attrib['alias'] if 'alias' in node.attrib else None,
        queues=list_attrib(node, 'queues'),
        renderpass=str_attrib(node, 'renderpass'),
        successcodes=list_attrib(node, 'successcodes'),
        errorcodes=list_attrib(node, 'errorcodes'),
        cmdbufferlevel=list_attrib(node, 'cmdbufferlevel')
    )


@dataclass(frozen=True)
class EnumValue:
    name: str
    value: str
    alias: str
    comment: str

    def __repr__(self):
        if self.alias:
            return f"{self.name} --> {self.alias}"
        return f"{self.name}: {self.value}" #  // {self.comment}


def parse_enum_value(node: ET.Element) -> EnumValue:
    return EnumValue(
        name=str_attrib(node, 'name'),
        value=str_attrib(node, 'value'),
        alias=str_attrib(node, 'alias'),
        comment=str_attrib(node, 'comment')
    )


@dataclass(frozen=True)
class Enum:
    name: str
    type: str
    values: List[EnumValue]

    def __repr__(self):
        values = '\n'.join(f'  - {p}' for p in self.values)
        return f'[enum] {self.name}\n{values}'


def parse_enum(node: ET.Element) -> Enum:
    return Enum(
        name=str_attrib(node, 'name'),
        type=str_attrib(node, 'type'),
        values=[parse_enum_value(vn) for vn in node.findall('enum')]
    )
