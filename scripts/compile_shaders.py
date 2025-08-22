#!/usr/local/bin/python3
import os
import re
import sys
import subprocess
import tempfile
import argparse
from pathlib import Path
from enum import Enum, auto
from dataclasses import dataclass
from collections import defaultdict

RED = "\033[31m"
YELLOW = "\033[33m"
RESET = "\033[0m"

class TokenType(Enum):
    POUND = auto()
    DOUBLE_L_BRACE = auto()
    DOUBLE_R_BRACE = auto()
    L_BRACE = auto()
    R_BRACE = auto()
    L_PAREN = auto()
    R_PAREN = auto()
    L_BRACKET = auto()
    R_BRACKET = auto()
    SEMICOLON = auto()
    COMMA = auto()
    PERIOD = auto()

    EQUALS = auto()
    PLUS = auto()
    PLUS_PLUS = auto()
    MINUS = auto()
    MINUS_MINUS = auto()
    ASTERISK = auto()
    SLASH = auto()

    IN = auto()
    OUT = auto()
    VERSION = auto()
    VOID = auto()
    UNIFORM = auto()
    SAMPLER = auto()
    SAMPLER2D = auto()
    TEXTURE2D = auto()
    IMAGE2D = auto()

    FLOAT = auto()
    VEC2 = auto()
    VEC3 = auto()
    VEC4 = auto()
    MAT2 = auto()
    MAT3 = auto()
    MAT4 = auto()

    DIRECTIVE_VERSION = auto()
    DIRECTIVE_LOCATION = auto()
    DIRECTIVE_SET_BINDING = auto()
    DIRECTIVE_PUSH_CONSTANT = auto()
    RATE_VERTEX = auto()
    RATE_INSTANCE = auto()
    BINDING = auto()
    OFFSET = auto()
    TIGHTLY_PACKED = auto()
    BUFFER_LABEL = auto()

    TEXT = auto()

directives = {
    TokenType.DIRECTIVE_VERSION,
    TokenType.DIRECTIVE_LOCATION,
    TokenType.DIRECTIVE_SET_BINDING,
    TokenType.DIRECTIVE_PUSH_CONSTANT,
}

SIZE_OF_FLOAT = 4
glsl_types_to_sizes = {
    TokenType.FLOAT: 1 * SIZE_OF_FLOAT,
    TokenType.VEC2: 2 * SIZE_OF_FLOAT,
    TokenType.VEC3 : 3 * SIZE_OF_FLOAT,
    TokenType.VEC4 : 4 * SIZE_OF_FLOAT,
    TokenType.MAT2 : 2 * 2 * SIZE_OF_FLOAT,
    TokenType.MAT3 : 3 * 3 * SIZE_OF_FLOAT,
    TokenType.MAT4 : 4 * 4 * SIZE_OF_FLOAT,
}

glsl_types_to_alignments = {
    TokenType.FLOAT: 4,
    TokenType.VEC2: 8,
    TokenType.VEC3 : 16,
    TokenType.VEC4 : 16,
    TokenType.MAT2 : 16,
    TokenType.MAT3 : 16,
    TokenType.MAT4 : 16,
}

glsl_types_to_c_types = {
    TokenType.FLOAT: "f32",
    TokenType.VEC2: "glm::vec2",
    TokenType.VEC3 : "glm::vec3",
    TokenType.VEC4 : "glm::vec4",
    TokenType.MAT2 : "glm::mat2",
    TokenType.MAT3 : "glm::mat3",
    TokenType.MAT4 : "glm::mat4",
}

text_to_glsl_keyword = {
    "in": TokenType.IN,
    "out": TokenType.OUT,
    "version": TokenType.VERSION,
    "void": TokenType.VOID,
    "uniform": TokenType.UNIFORM,
    "sampler": TokenType.SAMPLER,
    "sampler2D": TokenType.SAMPLER2D,
    "texture2D": TokenType.TEXTURE2D,
    "image2D": TokenType.IMAGE2D,

    "float": TokenType.FLOAT,
    "vec2": TokenType.VEC2,
    "vec3": TokenType.VEC3,
    "vec4": TokenType.VEC4,
    "mat2": TokenType.MAT2,
    "mat3": TokenType.MAT3,
    "mat4": TokenType.MAT4,

    "VERSION": TokenType.DIRECTIVE_VERSION,
    "LOCATION": TokenType.DIRECTIVE_LOCATION,
    "SET_BINDING": TokenType.DIRECTIVE_SET_BINDING,
    "PUSH_CONSTANT": TokenType.DIRECTIVE_PUSH_CONSTANT,
    "RATE_VERTEX": TokenType.RATE_VERTEX,
    "RATE_INSTANCE": TokenType.RATE_INSTANCE,
    "BINDING": TokenType.BINDING,
    "OFFSET": TokenType.OFFSET,
    "TIGHTLY_PACKED": TokenType.TIGHTLY_PACKED,
    "BUFFER_LABEL": TokenType.BUFFER_LABEL,
}

class Token():
    def __init__(self, type, start, text=None):
        self.type = type
        self.start = start
        self.text = text

def lex_string(s):
    tokens = []
    i = 0
    n = len(s)

    def skip_whitespace():
        nonlocal i 
        while i < n and s[i] in " \t\n":
            i += 1

    def lex_text():
        nonlocal i
        start = i
        while i < n and (s[i].isalnum() or s[i] == '_'):
            i += 1
        return s[start:i]


    while i < n:
        skip_whitespace()
        if i >= n:
            break

        if s[i] == '#':
            tokens.append(Token(TokenType.POUND, i))
            i += 1

        elif s[i] == '{':
            if i + 1 < n and s[i + 1] == '{':
                tokens.append(Token(TokenType.DOUBLE_L_BRACE, i))
                i += 2
            else:
                tokens.append(Token(TokenType.L_BRACE, i))
                i += 1

        elif s[i] == '}':
            if i + 1 < n and s[i + 1] == '}':
                tokens.append(Token(TokenType.DOUBLE_R_BRACE, i))
                i += 2
            else:
                tokens.append(Token(TokenType.R_BRACE, i))
                i += 1

        elif s[i] == '+':
            if i + 1 < n and s[i + 1] == '+':
                tokens.append(Token(TokenType.PLUS_PLUS, i))
                i += 2
            else:
                tokens.append(Token(TokenType.PLUS, i))
                i += 1

        elif s[i] == '-':
            if i + 1 < n and s[i + 1] == '-':
                tokens.append(Token(TokenType.MINUS_MINUS, i))
                i += 2
            else:
                tokens.append(Token(TokenType.MINUS, i))
                i += 1

        elif s[i] == '*':
            tokens.append(Token(TokenType.ASTERISK, i))
            i += 1
        elif s[i] == '/':
            if i + 1 < n and s[i+1] == '/':
                while i < n and s[i] != '\n':
                    i += 1
                i += 1
            else:
                tokens.append(Token(TokenType.SLASH, i))
                i += 1
        elif s[i] == '(':
            tokens.append(Token(TokenType.L_PAREN, i))
            i += 1
        elif s[i] == ')':
            tokens.append(Token(TokenType.R_PAREN, i))
            i += 1
        elif s[i] == '[':
            tokens.append(Token(TokenType.L_BRACKET, i))
            i += 1
        elif s[i] == ']':
            tokens.append(Token(TokenType.R_BRACKET, i))
            i += 1
        elif s[i] == ';':
            tokens.append(Token(TokenType.SEMICOLON, i))
            i += 1
        elif s[i] == ',':
            tokens.append(Token(TokenType.COMMA, i))
            i += 1
        elif s[i] == '.':
            tokens.append(Token(TokenType.PERIOD, i))
            i += 1
        elif s[i] == '=':
            tokens.append(Token(TokenType.EQUALS, i))
            i += 1
        else:
            start = i
            text = lex_text()
            if text in text_to_glsl_keyword:
                tokens.append(Token(text_to_glsl_keyword[text], start))
            else:
                tokens.append(Token(TokenType.TEXT, start, text))

    return tokens

# conceptualizing a vertex layout based on the location is asks for,
# the input rate, and the data type it expects at that location
# only supporting native glsl types, so using token types for
# glsl type and rate
@dataclass
class VertexAttribute:
    location: int
    binding: int
    glsl_type: TokenType
    rate: TokenType
    identifier: str
    offset: int
    is_tightly_packed: bool

@dataclass
class TemplateStringSlice:
    start: int
    end: int
    vulkan_replacement: str
    opengl_replacement: str

# TODO generalize SetBindingLayout to handle more descriptor types if necessary
class DescriptorType(Enum):
    SAMPLER2D = auto()
    UNIFORM_BUFFER = auto()

descriptor_type_to_vulkan_enum = {
    DescriptorType.SAMPLER2D: "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER",
    DescriptorType.UNIFORM_BUFFER: "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER"
}

# Not supporting nested structs or arrays yet
@dataclass
class StructMember:
    name: str
    glsl_type: TokenType
    offset: int
    size: int
    array_size: int

@dataclass
# not supporting anonymous structs yet
class StructDescription:
    typename: str
    identifier: str
    size: int
    members: list[StructMember]

@dataclass
class SetBindingLayout:
    set_id: int
    binding: int
    # TODO deprecate is_sampler
    is_sampler: bool
    type: DescriptorType
    struct_description: StructDescription
    buffer_label: str

def align_up(offset, alignment):
    return (offset + alignment - 1) & ~(alignment - 1)

def parse_tokens(tokens, stage, parser_error_reporter):
    i = 0
    current_start = 0
    n = len(tokens)
    slices = []
    locations = []
    set_bindings = []

    def parse_version_directive():
        nonlocal i
        nonlocal current_start

        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.DIRECTIVE_VERSION, i, 
                              "Expected DIRECTIVE_VERSION")
        if not still_valid:
            return
        i += 1

        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.DOUBLE_R_BRACE, i, 
                              "Expected }} in VERSION directive")
        if not still_valid:
            return
        i += 1

        slices.append(TemplateStringSlice(current_start, tokens[i].start, vulkan_version_string(), opengl_version_string()))
        current_start = tokens[i].start

    def parse_glsl_struct():
        nonlocal i

        assert(tokens[i].type == TokenType.TEXT)
        uniform_structure_type_name = tokens[i].text 
        i += 1
        size = 0
        members = []
        identifier = None

        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.L_BRACE, i,
                              "Expected { to begin uniform struct definition")
        i += 1

        while i < n and tokens[i].type != TokenType.R_BRACE:

            (i, still_valid) = parser_error_reporter(tokens[i].type in glsl_types_to_alignments, i,
                                  "Expected glsl type in uniform layout")
            (i, still_valid) = parser_error_reporter(tokens[i].type != TokenType.VEC3, i,
                                  "Found vec3 inside glsl struct, not supported because alignment hell")

            glsl_type = tokens[i].type
            member_size = glsl_types_to_sizes[tokens[i].type]
            current_alignment = glsl_types_to_alignments[tokens[i].type]

            if not still_valid:
                return
            i += 1

            current_size = members[-1].offset + members[-1].size if members else 0
            offset = align_up(current_size, current_alignment)

            (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.TEXT, i,
                                  "Expected identifier after glsl type in uniform")
            if not still_valid:
                return

            name = tokens[i].text
            i += 1

            array_size = 0;
            if tokens[i].type == TokenType.L_BRACKET:
                i += 1

                (i, still_valid) = parser_error_reporter(tokens[i].text.isdigit(), i, 
                                      "Expected digit after [ in struct array")
                if not still_valid:
                    return

                array_size = int(tokens[i].text)
                i += 1

                (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.R_BRACKET, i, 
                                      "Expected ] after array size in struct array")
                if not still_valid:
                    return

                i += 1


            (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.SEMICOLON, i,
                                  "Expected ; after identifier or array in uniform")
            if not still_valid:
                return
            i += 1

            members.append(StructMember(name, glsl_type, offset, member_size, array_size))

        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.R_BRACE, i,
                              "Expected } to finish uniform struct definition")
        i += 1

        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.TEXT, i,
                              "Expected identifier after glsl uniform description")
        if not still_valid:
            return
        identifier = tokens[i].text
        i += 1

        if members:
            final_offset = members[-1].offset + members[-1].size
            final_alignment = max(glsl_types_to_alignments[member.glsl_type] for member in members)
            size = align_up(final_offset, final_alignment)

            if size > final_offset:
                bytes_to_pad = (size - final_offset)
                num_floats =  bytes_to_pad // 4
                members.append(StructMember(f"_pad[{num_floats}]", TokenType.FLOAT, final_offset, bytes_to_pad))

        return StructDescription(uniform_structure_type_name, identifier, size, members)

    # {{ LOCATION N BINDING M RATE OFFSET (n | TIGHTLY_PACKED)}} (in | out) type identifier;
    def parse_location_directive():
        nonlocal i
        nonlocal current_start

        # LOCATION
        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.DIRECTIVE_LOCATION, i, 
                              "Expected DIRECTIVE_LOCATION")
        if not still_valid:
            return
        i += 1

        # N
        (i, still_valid) = parser_error_reporter(tokens[i].text.isdigit(), i, 
                              "Expected digit after LOCATION")
        if not still_valid:
            return
        location = int(tokens[i].text)
        i += 1

        # (RATE)
        is_vertex_stage = (stage == ShaderStage.VERTEX)
        # in non vertex stages, only specify locations, no vertex layouts
        if not is_vertex_stage:
            (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.DOUBLE_R_BRACE, i, 
                                  "In non-vertex shader LOCATION directive, not of form {{ LOCATION N }}")
            if not still_valid:
                return

            i += 1
            slices.append(TemplateStringSlice(current_start, tokens[i].start, vulkan_location_string(location), opengl_location_string()))
            return

        # if in a vertex stage, can either be an out, handled here and returned early,
        # or a fully layout, parsed after
        if tokens[i].type == TokenType.DOUBLE_R_BRACE:
            i += 1
            slices.append(TemplateStringSlice(current_start, tokens[i].start, vulkan_location_string(location), opengl_location_string()))
            (i, still_valid) = parser_error_reporter(tokens[i].type != TokenType.IN, i, 
                              "In vertex shader LOCATION directive of form {{ LOCATION N }} (not in), expected full vertex layout")
            return

        rate = None
        offset = None
        is_tightly_packed = None
        # BINDING
        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.BINDING, i, 
                              "Expected BINDING after location")
        if not still_valid:
            return
        i += 1

        # binding id
        (i, still_valid) = parser_error_reporter(tokens[i].text.isdigit(), i, 
                              "Expected digit after BINDING")
        if not still_valid:
            return
        binding = int(tokens[i].text)
        i += 1

        # rate
        is_rate = (tokens[i].type == TokenType.RATE_VERTEX) or (tokens[i].type == TokenType.RATE_INSTANCE)
        (i, still_valid) = parser_error_reporter(is_rate, i, 
                              "After Binding ID, RATE expected")
        if not still_valid:
            return
        rate = tokens[i].type
        i += 1

        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.OFFSET, i, 
                              "Expected OFFSET after RATE in Location directive")
        if not still_valid:
            return
        i += 1

        is_tightly_packed = (tokens[i].type == TokenType.TIGHTLY_PACKED)
        if is_tightly_packed:
            offset = 0
        else:
            (i, still_valid) = parser_error_reporter(tokens[i].text.isdigit(), i, 
                                  "Expected digit or TIGHTLY_PACKED after OFFSET")
            if not still_valid:
                return
            offset = int(tokens[i].text)

        i += 1


        # }}
        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.DOUBLE_R_BRACE, i, 
                              "Expected }} after location directive")
        if not still_valid:
            return
        i += 1
        slices.append(TemplateStringSlice(current_start, tokens[i].start, vulkan_location_string(location), opengl_location_string()))
        current_start = tokens[i].start

        # ( in | out )
        is_in = (tokens[i].type == TokenType.IN)
        is_out = (tokens[i].type == TokenType.OUT)
        (i, still_valid) = parser_error_reporter(is_in or is_out, i, "Expected in or out after location directive")
        if not still_valid:
            return
        is_vertex_layout = (tokens[i].type == TokenType.IN)
        i += 1

        # type
        (i, still_valid) = parser_error_reporter(tokens[i].type in glsl_types_to_sizes, i,
                              "Expected glsl type after in/out in location description")
        if not still_valid:
            return
        glsl_type = tokens[i].type
        i += 1

        # identifier
        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.TEXT, i,
                              "Expected identifier after location")
        if not still_valid:
            return
        identifier = tokens[i].text
        i += 1

        if is_in and stage == ShaderStage.VERTEX:
            if offset is None or is_tightly_packed is None or rate is None:
                none_fields = ""
                if offset is None:
                    none_fields += "offset"

                if is_tightly_packed is None:
                    if none_fields:
                        none_fields += ", "
                    none_fields += "is_tightly_packed"

                if rate is None:
                    if none_fields:
                        none_fields += ", "
                    none_fields += "rate"

                conjugated_be = "are" if "," in none_fields else "is"
                (i, still_valid) = parser_error_reporter(False, i,
                                      f"Parsing in attribute in vertex shader, but {none_fields} {conjugated_be} None")
                if not still_valid:
                    return
                i += 1

            else:
                locations.append(VertexAttribute(
                    location, binding, glsl_type, rate, identifier, offset, is_tightly_packed)
                                 )


        # ;
        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.SEMICOLON, i,
                              "Expected ; at end of location line")
        if not still_valid:
            return
        i += 1


    # {{ SET_BINDING set binding }} uniform sampler2D identifier;
    # or 
    # {{ SET_BINDING set binding BUFFER_LABEL label}} uniform identifier { (type identifier;)! } idenitifer;
    def parse_set_binding_directive():
        nonlocal i
        nonlocal current_start

        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.DIRECTIVE_SET_BINDING, i,
                              "Expected SET_BINDING")
        if not still_valid:
            return
        i += 1

        # need to call this set_id bc set is reserved in python
        (i, still_valid) = parser_error_reporter(tokens[i].text.isdigit(), i,
                              "Expected set id digit after SET_BINDING")
        if not still_valid:
            return
        set_id = int(tokens[i].text)
        i += 1

        # binding
        (i, still_valid) = parser_error_reporter(tokens[i].text.isdigit(), i,
                              "Expected binding id digit after SET_BINDING set_id")
        if not still_valid:
            return
        binding = int(tokens[i].text)
        i += 1

        buffer_label = None
        if tokens[i].type == TokenType.BUFFER_LABEL:
            i += 1
            (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.TEXT, i,
                                  "Expected identifier after BUFFER_LABEL")
            if not still_valid:
                return
            buffer_label = tokens[i].text
            i += 1

        # }}
        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.DOUBLE_R_BRACE, i,
                              "Expected }} to close SET_BINDING")
        if not still_valid:
            return
        i += 1
        slices.append(TemplateStringSlice(current_start, tokens[i].start, vulkan_set_binding_string(set_id, binding), opengl_set_binding_string()))
        current_start = tokens[i].start

        (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.UNIFORM, i,
                              "Expected uniform after SET_BINDING directive")
        if not still_valid:
            return
        i += 1

        # don't think I need extra info here for samplers
        # {{ ... }} uniform sampler2D identifier;
        # {{ ... }} uniform identifier { ... } identifier;
        # the first identifier in the non sampler is a type name, the second is the instance identifier 
        is_sampler = (tokens[i].type == TokenType.SAMPLER2D)
        # TODO generalize if/when more descriptor types are supported
        descriptor_type = DescriptorType.SAMPLER2D if is_sampler else DescriptorType.UNIFORM_BUFFER 

        if buffer_label is not None:
            (_, still_valid) = parser_error_reporter(not is_sampler, i,
                                  "Specified a buffer label for a sampler")
            if not still_valid:
                return

        struct_description = None
        # TODO would be better to think about this in terms of is uniform instead of not sampler
        if not is_sampler:
            (i, still_valid) = parser_error_reporter(tokens[i].type == TokenType.TEXT, i,
                                  "Expected structure type name after uniform in a non-sampler")
            if not still_valid:
                return

            struct_description = parse_glsl_struct()
            (i, still_valid) = parser_error_reporter(struct_description is not None, i,
                                  "Failed to parse struct description in set/binding directive")
            if not still_valid:
                return

        else:
            # skip the sampler2D
            i += 1

        set_bindings.append(SetBindingLayout(set_id, binding, is_sampler, descriptor_type, struct_description, buffer_label))

    def parse_push_constant_directive():
        pass

    while i < n:
        if tokens[i].type != TokenType.DOUBLE_L_BRACE:
            i += 1
            continue

        # skip {{, and track where it started. Only record slices we need to replace
        current_start = tokens[i].start
        i += 1
        if i >= n:
            print("parse_tokens: EOF after {{")
            exit(1)
        if tokens[i].type not in directives:
            print(f"parse_tokens: expected directive after {{{{, got {tokens[i].type}")
            exit(1)

        directive_type = tokens[i].type
        if directive_type == TokenType.DIRECTIVE_LOCATION:
            parse_location_directive()
        elif directive_type == TokenType.DIRECTIVE_VERSION:
            parse_version_directive()
        elif directive_type == TokenType.DIRECTIVE_SET_BINDING:
            parse_set_binding_directive()
        elif directive_type == TokenType.PUSH_CONSTANT:
            parse_push_constant_directive()

    return (slices, locations, set_bindings)

def vulkan_version_string():
    return "450\n"

def opengl_version_string():
    return "410 core\n"

def vulkan_location_string(location):
    assert(location is not None)
    return f"layout(location = {location}) "

def opengl_location_string():
    return ""

def vulkan_set_binding_string(set_id, binding):
    assert(set_id is not None)
    assert(binding is not None)
    return f"layout(set = {set_id}, binding = {binding}) "

def opengl_set_binding_string():
    return ""

class VulkanFormat(Enum):
    VK_FORMAT_R32G32_SFLOAT = "VK_FORMAT_R32G32_SFLOAT" 
    VK_FORMAT_R32G32B32_SFLOAT = "VK_FORMAT_R32G32B32_SFLOAT" 

glsl_type_to_vulkan_format = {
    TokenType.VEC2: VulkanFormat.VK_FORMAT_R32G32_SFLOAT,
    TokenType.VEC3: VulkanFormat.VK_FORMAT_R32G32B32_SFLOAT 
}

vulkan_format_to_glsl_type = {
    VulkanFormat.VK_FORMAT_R32G32_SFLOAT: "VEC2",
    VulkanFormat.VK_FORMAT_R32G32B32_SFLOAT: "VEC3",
}

@dataclass
class VulkanVertexAttribute:
    location: int
    binding: int
    format: VulkanFormat
    offset: int

@dataclass
class VulkanVertexBinding:
    binding: int
    stride: int
    rate: TokenType

@dataclass
class VulkanVertexLayout:
    attributes: list[VulkanVertexAttribute]
    bindings: list[VulkanVertexBinding]

@dataclass
class VertexAttribute:
    location: int
    binding: int
    glsl_type: TokenType
    rate: TokenType
    identifier: str
    offset: int
    is_tightly_packed: bool

# vertex_layouts: list of VertexAttribute
# this function converts a templated vertex attribute from glsl
# into the information needed to output a vulkan C side vertex binding/attribute 
# this is done per shader
def generate_vertex_bindings_and_attributes(vertex_attributes):
    if len(vertex_attributes) == 0:
        return None

    attributes = []
    locations = set()
    binding_id_to_stride_and_rate = {}
    binding_offsets = defaultdict(int)

    is_tightly_packed = vertex_attributes[0].is_tightly_packed

    for attr in vertex_attributes:
        if attr.is_tightly_packed != is_tightly_packed:
            print(f"{RED}Vertex attributes inconsistent in packing{RESET}")
            return None

        if attr.location in locations:
            print(f"{RED}Repeat location {attr.location}{RESET}")
            return None
        locations.add(attr.location)

        format = glsl_type_to_vulkan_format[attr.glsl_type]
        binding = attr.binding

        size = glsl_types_to_sizes[attr.glsl_type]
        offset = attr.offset
        if is_tightly_packed:
            offset = binding_offsets[binding]
            binding_offsets[binding] += size

        attribute = VulkanVertexAttribute(attr.location, binding, format, offset)

        # am assuming that I will basically always pack my data tightly, and that
        # stride will be the sum of the sizes of the types in that binding
        if attr.binding not in binding_id_to_stride_and_rate:
            binding_id_to_stride_and_rate[binding] = VulkanVertexBinding(binding, size, attr.rate)
        else:
            binding_id_to_stride_and_rate[attr.binding].stride += size
            if binding_id_to_stride_and_rate[attr.binding].rate != attr.rate:
                print(f"{RED}Vertex bindings inconsistent in rate{RESET}")
                return None

        attributes.append(attribute)

    bindings = sorted(binding_id_to_stride_and_rate.values(), key=lambda b: b.binding)
    attributes.sort(key=lambda x: (x.binding, x.location))
    return VulkanVertexLayout(attributes, bindings)

# global_vertex_layouts list  of VulkanVertexLayout
def check_vertex_layout_present(global_vertex_layouts, new_vertex_layout):
    # if new layout is the same as an existing layout, return true, and in the caller,
    # don't add this new layout to the global vertex table
    for existing_layout in global_vertex_layouts:
        if len(new_vertex_layout.bindings) != len(existing_layout.bindings):
            continue
        if len(new_vertex_layout.attributes) != len(existing_layout.attributes):
            continue

        all_bindings_equal = True
        for i in range(len(new_vertex_layout.bindings)):
            if new_vertex_layout.bindings[i] != existing_layout.bindings[i]:
                all_bindings_equal = False
                break

        all_attributes_equal = True
        for i in range(len(new_vertex_layout.attributes)):
            if new_vertex_layout.attributes[i] != existing_layout.attributes[i]:
                all_attributes_equal = False
                break

        if all_attributes_equal and all_bindings_equal:
            return True

    return False


# shader_stage is an enum
# entry point into parser, gets slices where replacements need to happen,
# populates the global symbol tables: global_vertex_layouts, global_descriptor_sets
# handles vertex descriptions through a side effect, returns strings containing
# opengl/vulkan source
def compile_shader_file(filepath, name, shader_stage, global_vertex_layouts, global_descriptor_sets):
    with open(filepath, 'r', encoding='utf-8') as f:
        contents = f.read()

    tokens = lex_string(contents)

    # dont advance index if condition is true
    # if condition is false, report an error and recover
    def parser_error_reporter(condition, token_idx, message):
        if not condition:
            print(f"{RED}Parser error in {filepath}{RESET}")
            char_idx = tokens[token_idx].start
            start = char_idx
            end = char_idx

            while start > 0 and contents[start] != '\n':
                start -= 1
            if contents[start] == '\n':
                start += 1

            while end < len(contents) and contents[end] != '\n':
                end += 1

            print(f"\t{contents[start:end]}")
            print(message)

            while token_idx < len(tokens) and tokens[token_idx].type != TokenType.DOUBLE_R_BRACE:
                token_idx += 1
            return (token_idx, False)

        return (token_idx, True)

    slices, vertex_layouts, set_bindings = parse_tokens(tokens, shader_stage, parser_error_reporter)

    vertex_layout_enum_name = "INVALID_VERTEX_LAYOUT"
    if shader_stage == ShaderStage.VERTEX:
        if len(vertex_layouts) == 0:
            print(f"{YELLOW} Warning: Parsed vertex shader {filepath} with no layouts {RESET}")
        else:
            vertex_layout = generate_vertex_bindings_and_attributes(vertex_layouts)
            if not check_vertex_layout_present(global_vertex_layouts, vertex_layout):
                global_vertex_layouts.append(vertex_layout)
            vertex_layout_enum_name = make_vertex_layout_name(vertex_layout)

    global_descriptor_sets.append(set_bindings)

    vulkan_source = ""
    opengl_source = ""
    current_start = 0
    for slice in slices:
        debug_slice = contents[slice.start:slice.end]
        assert slice.vulkan_replacement is not None, f"Missing Vulkan replacement at {filepath} {slice.start}:{slice.end},\n\t {debug_slice}"
        assert slice.opengl_replacement is not None, f"Missing OpenGL replacement at {filepath} {slice.start}:{slice.end},\n\t {debug_slice}"
        # append the glsl source from the end of the last directive up to the start of this one
        vulkan_source += contents[current_start:slice.start]
        opengl_source += contents[current_start:slice.start]
        current_start = slice.end 

        vulkan_source += slice.vulkan_replacement
        opengl_source += slice.opengl_replacement

    vulkan_source += contents[current_start:]
    opengl_source += contents[current_start:]
    spirv = compile_to_spirv_and_get_bytes(vulkan_source, shader_stage)

    return CompiledShader(name, spirv, shader_stage, opengl_source, vertex_layout_enum_name)

# returns the result of parsing and compiling all shaders
# defines global tables
def compile_all_shaders(shaders):
    global_vertex_layouts = []
    global_descriptor_sets = []
    code = ""
    vertex_layout_enum_names = []

    code += "// Generated shader header, do not edit\n"
    code += "#pragma once\n#include <stdint.h>\n#include <stddef.h>\n#include \"vulkan_base.h\"\n\n"

    shader_spec_array_code = f"const u32 num_generated_specs = {len(shaders)};\n"
    # TODO put in .cpp when separating header and cpp
    shader_spec_array_code += "static const ShaderSpec* generated_shader_specs[] = {"

    shader_specifications_code = ""

    for source_file, name, stage in shaders:
        shader = compile_shader_file(source_file, name, stage, global_vertex_layouts, global_descriptor_sets)

        enum_name = shader.vertex_layout_enum_name 
        if enum_name not in vertex_layout_enum_names and enum_name != 'INVALID_VERTEX_LAYOUT':
            vertex_layout_enum_names.append(shader.vertex_layout_enum_name)
        shader_specifications_code += shader_spec_codegen(shader)
        shader_spec_array_code += f"\n  &{name}_spec,"

    code += vertex_layout_codegen(global_vertex_layouts, vertex_layout_enum_names)
    code += '\n\n'

    code += shader_specifications_code
    code += '\n\n'

    code += descriptor_set_codegen(global_descriptor_sets)
    code += '\n\n'

    shader_spec_array_code += "\n};\n"
    code += shader_spec_array_code
    return code

def make_vertex_layout_name(vertex_layout):
    attributes, bindings = vertex_layout.attributes, vertex_layout.bindings

    unique_bindings_to_rates = {}
    for binding in bindings:
        if binding.binding in unique_bindings_to_rates:
            if unique_bindings_to_rates[binding.binding] != binding.rate:
                print(f"{RED}Mismatch in rates for binding {binding.binding}{RESET}")
                return None
        else:
            unique_bindings_to_rates[binding.binding] = binding.rate

    # the name is determined by the locations and uniqueness of bindings
    # if there is only a single vertex binding, then skip marking all the bindings
    only_one_vertex_rate_binding = False
    enum_name = "VERTEX_LAYOUT"
    if len(unique_bindings_to_rates) == 1:
        for rate in unique_bindings_to_rates.values():
            only_one_vertex_rate_binding =  (rate == TokenType.RATE_VERTEX)

    only_vertex_rate_bindings = True
    for rate in unique_bindings_to_rates.values():
        if (rate == TokenType.RATE_INSTANCE):
            only_vertex_rate_bindings = False

    current_binding = None
    for attribute in attributes:
        if attribute.binding != current_binding:
            if not only_one_vertex_rate_binding:
                enum_name += f"_BINDING{attribute.binding}"
            if not only_vertex_rate_bindings:
                if unique_bindings_to_rates[attribute.binding] == TokenType.RATE_VERTEX:
                    enum_name += f"_RATE_VERTEX"
                if unique_bindings_to_rates[attribute.binding] == TokenType.RATE_INSTANCE:
                    enum_name += f"_RATE_INSTANCE"
            current_binding = attribute.binding

        enum_name += f"_{vulkan_format_to_glsl_type[attribute.format]}"

    return enum_name


def vertex_layout_codegen(global_vertex_layouts, enum_names):
    enum_code = "enum GeneratedVertexLayoutID {\n"

    for enum_name in enum_names:
        enum_code += f"  {enum_name},\n"

    enum_code += "  NUM_GENERATED_VERTEX_LAYOUTS,\n  INVALID_VERTEX_LAYOUT\n};\n\n"

    array_code = "const VulkanVertexLayout generated_vertex_layouts[NUM_GENERATED_VERTEX_LAYOUTS] = {\n"
    for i in range(len(global_vertex_layouts)):
        vertex_layout = global_vertex_layouts[i]
        attributes = vertex_layout.attributes
        bindings = vertex_layout.bindings

        code = f"  [{enum_names[i]}] = {{\n"
        code += f"    .binding_count = {len(bindings)},\n"
        code += f"    .bindings = {{\n"

        for binding in bindings:
            rate_string = ""
            if binding.rate == TokenType.RATE_VERTEX:
                rate_string = "VK_VERTEX_INPUT_RATE_VERTEX"
            if binding.rate == TokenType.RATE_INSTANCE:
                rate_string = "VK_VERTEX_INPUT_RATE_INSTANCE"

            code += f"      {{ .binding = {binding.binding}, .stride = {binding.stride}, .input_rate = {rate_string} }},\n"

        code += f"    }},\n"

        code += f"    .attribute_count = {len(attributes)},\n"
        code += f"    .attributes = {{\n"
        for attribute in attributes:
            code += f"      {{ .location = {attribute.location}, .binding = {attribute.binding}, .format = {attribute.format.value}, .offset = {attribute.offset} }},\n"
        code += f"    }}\n"

        code += f"  }},\n"
        array_code += code

    array_code += "};\n\n"

# TODO separate header and implementation to make a truly global registry
# need to get rid of the static qualifiers
    registry_code = r"""static VertexLayout _vertex_layout_registry[NUM_GENERATED_VERTEX_LAYOUTS];
inline void init_vertex_layout_registry(){
  for(u32 i = 0; i < NUM_GENERATED_VERTEX_LAYOUTS; i++){
    push_vertex_attributes_and_bindings_and_finalize(&_vertex_layout_registry[i], generated_vertex_layouts[i]);
  }
}

static const VertexLayout *const vertex_layout_registry = _vertex_layout_registry;

inline const VkPipelineVertexInputStateCreateInfo* get_vertex_layout(GeneratedVertexLayoutID id){
  assert(id < NUM_GENERATED_VERTEX_LAYOUTS);
  return &vertex_layout_registry[id].vertex_input_state;
}
"""

    shader_spec_definitions = r"""struct ShaderSpec {
  const u32 *spv;
  u32 size;
  const char *name;
  VkShaderStageFlagBits stage_flags;
  GeneratedVertexLayoutID vertex_layout_id;
};

static bool cache_shader_module(VulkanShaderCache *cache, ShaderSpec spec) {

  u32 table_index = cache->hash_map.probe(spec.name);
  if (cache->hash_map.key_values[table_index].occupancy == HASHMAP_OCCUPIED) {
    fprintf(stderr,
            "cache_shader_module: Attempting to overwrite shader module. "
            "Attempting to cache %s, overwriting %s\n",
            spec.name, cache->hash_map.key_values[table_index].key);
    return false;
  }

  if (table_index == cache->hash_map.capacity) {
    fprintf(stderr,
            "cache_shader_module: probe returned table.capacity when hashing "
            "\"%s\" - table full. table index: %d, capacity %d\n",
            spec.name, table_index, cache->hash_map.capacity);
    return false;
  }

  // shaderstage: VkShaderStageFlagBits
  // shaderspec: VkShaderStageFlags
  ShaderStage *stage = &cache->hash_map.key_values[table_index].value;
  stage->module = create_shader_module(cache->device, spec.spv, spec.size);
  stage->stage = spec.stage_flags;
  // TODO add preprocessing to shader for specifying an entry point
  stage->entry_point = "main";

  cache->hash_map.key_values[table_index].key = spec.name;
  cache->hash_map.key_values[table_index].occupancy = HASHMAP_OCCUPIED;
  cache->hash_map.size++;

  return true;
}

static bool cache_shader_modules(VulkanShaderCache *cache, const ShaderSpec **specs,
                          u32 num_specs) {
  for (u32 i = 0; i < num_specs; i++) {
    if (!cache_shader_module(cache, *specs[i])) {
      return false;
    }
  }
  return true;
}


"""
    return enum_code + shader_spec_definitions + array_code + registry_code

# different from vertex layouts, where we check all vertex layouts when adding a new one
# here, it only matters if two structs with the same name have different layouts, because
# that presents an ambiguity for the C compiler when it tries to compile our code
def check_structs_the_same(new_desc, old_desc):
    return new_desc.members == old_desc.members

# global_descriptor_sets: list of SetBindingLayout
def descriptor_set_codegen(global_descriptor_sets):
    for s in global_descriptor_sets:
        print("--------")
        for it in s:
            print(it)

    # currently not supporting anonymous structs, but map typename to description anyway
    # that's what I would do if I had a proper NamedStruct structure myself
    descriptor_type_counts = {}
    structs = {}

    # each unique BUFFER_LABEL gets a uniform buffer
    # each unique identifier gets an entry into that uniform buffer
    # e.g. uniform MVPUniform {...} mvp;, if repeated in two shaders
    # with {{ ... BUFFER_LABEL GLOBAL }} go into the global uniform buffer
    # model each uniform buffer as a list of named structures. track an offset within the buffer per struct
    uniform_buffers = {}

    for descriptor_set_list in global_descriptor_sets:
        for descriptor_set in descriptor_set_list:
            assert(descriptor_set.type in descriptor_type_to_vulkan_enum)
            # descriptor type counts tracks info for descriptor pools - how many resources of a given type 
            if descriptor_set.type not in descriptor_type_counts:
                descriptor_type_counts[descriptor_set.type] = 1
            else:
                descriptor_type_counts[descriptor_set.type] += 1

            # tracking glsl struct definitions to emit a C translation
            if descriptor_set.struct_description:
                struct_desc = descriptor_set.struct_description
                if struct_desc.typename in structs:
                    if not check_structs_the_same(struct_desc, structs[struct_desc.typename]):
                        print(f"{RED} found colliding struct defintions for {struct_desc.typename}{RESET}")
                else:
                    structs[struct_desc.typename] = struct_desc

    c_struct_definitions = ""

    # emit struct body
    for typename, struct in structs.items():
        max_alignment = max(glsl_types_to_alignments[m.glsl_type] for m in struct.members)

        struct_array_sizes = ""
        # TODO figure out if this should ever not just be 16
        struct_definition = f"struct alignas(16) {typename} {{\n"

        max_alignment = 0
        for member in struct.members:
            if member.array_size:
                size_variable_name = f"{typename}_{member.name}_array_size"
                struct_array_sizes += f"const u32 {size_variable_name} = {member.array_size};\n"
                array_snippet = f"[{size_variable_name}]"
            else:
                array_snippet = ""
                size_variable_name = ""

            c_type = glsl_types_to_c_types[member.glsl_type]
            alignment = glsl_types_to_alignments[member.glsl_type]
            struct_definition += f"  alignas({alignment}) {c_type} {member.name}{array_snippet};\n"

        struct_definition += "};\n\n"

        c_struct_definitions += struct_array_sizes
        c_struct_definitions += struct_definition

    # descriptor pool creation codegen
    num_descriptor_types = len(descriptor_type_counts)
    descriptor_pool_code = f"const u32 pool_size_count = {num_descriptor_types};\n"
    descriptor_pool_code += f"const VkDescriptorPoolSize generated_pool_sizes[{num_descriptor_types}] = {{\n"

    i = 0
    max_sets = 0
    for descriptor, count in descriptor_type_counts.items():
        descriptor_pool_code += f"  {{ .type = {descriptor_type_to_vulkan_enum[descriptor]}, .descriptorCount = {count}}},\n"

        max_sets = max(max_sets, count)
        i += 1

    descriptor_pool_code += "};\n"
    descriptor_pool_code += f"const u32 max_sets = {max_sets};\n"

    print(uniform_buffers)

    return c_struct_definitions + descriptor_pool_code

def source_is_newer(source, target):
    if not target.exists():
        return True
    return source.stat().st_mtime > target.stat().st_mtime

# expect name.{vert/frag/comp}.in
def validate_filename_and_get_parts(filename):
    parts = filename.split(".")
    stage_is_valid = parts[1] in string_to_shader_stage  
    if not (len(parts) == 3 and stage_is_valid and parts[2] == "in"):
        print("Expected filename of the form {{name}}.{{vert/frag/comp}}.{{in}}")
        print("\tNot compiling file: " + filename)
        return None
    return parts

# write source code to a temp file, compile that temp file into spv, and return array with bytecode
def compile_to_spirv_and_get_bytes(source_code, stage):
    if args.dump_vulkan_source:
        print(f"dumping source")
        print(source_code)

    # write glsl to file
    with tempfile.NamedTemporaryFile(suffix=f".{stage.value}", delete=False, mode='w', encoding='utf-8') as tmp_file:
        tmp_file.write(source_code)
        glsl_path = tmp_file.name

    # will need to read spv from other temp file
    with tempfile.NamedTemporaryFile(suffix=f".{stage.value}", delete=False) as tmp_file:
        spv_path = tmp_file.name

    try:
        result = subprocess.run([
            "glslangValidator",
            "-S", stage.value,
            "-o", spv_path,
            "-V", glsl_path
        ], capture_output=True, text=True)

        if result.returncode != 0:
            print("Compilation failed. Source dump:")
            print(source_code)
            print(result.stdout)
            print(result.stderr)
            return None
        else:
            return get_spirv_bytes(spv_path)
    finally:
        os.remove(glsl_path)
        os.remove(spv_path)

# after writing the spirv bytecode to a file, read the file and return the bytecode
# returns a bytes object, not a string
# validates by requiring 4 bytes words
def get_spirv_bytes(path):
    with open(path, "rb") as spirv_f:
        spirv = spirv_f.read()

    if len(spirv) % 4 != 0:
        print(f"\tThe length of {path} spirv is {len(spirv)}, not a multiple of 4")
        print(f"\tNot writing {path} to c header")
        return None

    return spirv

class ShaderStage(Enum):
    VERTEX = "vert"
    FRAGMENT = "frag"
    COMPUTE = "comp"

string_to_shader_stage = {
    "vert": ShaderStage.VERTEX,
    "frag": ShaderStage.FRAGMENT,
    "comp": ShaderStage.COMPUTE,
}

file_stage_to_flag = {
    ShaderStage.VERTEX: "VK_SHADER_STAGE_VERTEX_BIT",
    ShaderStage.FRAGMENT: "VK_SHADER_STAGE_FRAGMENT_BIT",
    ShaderStage.COMPUTE: "VK_SHADER_STAGE_COMPUTE_BIT",
}

@dataclass
class CompiledShader:
    name: str
    spirv: bytes
    stage: ShaderStage
    opengl_source: str
    vertex_layout_enum_name: str

def c_multiline_string_literal(s):
    lines = s.splitlines()
    quoted_lines = ['"' + line.replace('"', '\\"') + '"' for line in lines]
    return "\n".join(quoted_lines)

# produces the ShaderSpec for a single shader
# shader: Shader
def shader_spec_codegen(shader):
    lines = []

    u32_words = [int.from_bytes(shader.spirv[i:i+4], "little") for i in range(0, len(shader.spirv), 4)]
    stage_flags = file_stage_to_flag[shader.stage]

    lines.append(f"static const uint32_t {shader.name}[] = {{")
    for i in range(0, len(u32_words), 4):
        chunk = ", ".join(f"0x{w:08x}" for w in u32_words[i:i+4])
        lines.append(f"    {chunk},")
    lines.append("};\n")
    lines.append(f"static const size_t {shader.name}_size = sizeof({shader.name});")
    lines.append(f"static constexpr const char* {shader.name}_name = \"{shader.name}\";")

    # need to output a constexpr const struct like this with the .initialization because
    # these assignments don't occur inside a function, thank you C++
    lines.append(f"constexpr const ShaderSpec {shader.name}_spec = {{")
    lines.append(f"  .spv = {shader.name},")
    lines.append(f"  .size = sizeof({shader.name}),")
    lines.append(f"  .name = {shader.name}_name,")
    lines.append(f"  .stage_flags = {stage_flags},")
    lines.append(f"  .vertex_layout_id = {shader.vertex_layout_enum_name},")
    lines.append("};\n")

    c_string = c_multiline_string_literal(shader.opengl_source)
    lines.append(f"static constexpr const char* {shader.name}_opengl_glsl = {c_string};\n")

    return "\n".join(lines)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dump-vulkan-source",
        action="store_true",
        help="Print vulkan source when compiling to glsl 450"
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Ignore timestamps and force compilation"
    )
    parser.add_argument(
        "--subdir",
        type=str,
        help="subdirectory of shaders/ to compile"
    )
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.realpath(__file__))
    project_root = Path(os.path.abspath(os.path.join(script_dir, "..")))
    shaders_base_dir = project_root / "shaders"
    generated_header_name = "compiled_shaders.h"

    if args.subdir:
        shaders_dir = shaders_base_dir / args.subdir
        if not shaders_dir.exists() or not shaders_dir.is_dir():
            print(f"Error: subdirectory '{args.subdir}' does not exist in 'shaders/'")
            sys.exit(1)
        generated_header_name = f"{args.subdir}_compiled_shaders.h"
    else:
        shaders_dir = shaders_base_dir

    # header gen dir is where the generated C code goes, for use in the engine
    header_gen_dir = project_root / "gen"
    header_to_generate_path = header_gen_dir / generated_header_name

    if not os.path.isdir(shaders_dir):
        print(f"Error: Could not find shaders directory at '{shaders_dir}'")
        sys.exit(1)

    shader_subdirectories = [x[0] for x in os.walk(shaders_dir) if os.path.basename(x[0]) != "gen"]
    os.makedirs(header_gen_dir, exist_ok=True)

    # using a monolithic single output header for all shaders
    shaders_to_compile = []
    global_vertex_layouts = {}
    should_recompile = False
    for subdir in shader_subdirectories:
        files = [os.path.abspath(os.path.join(subdir, f)) for f in os.listdir(subdir) if os.path.isfile(os.path.join(subdir, f))]

        for source_file in files:
            if source_is_newer(Path(source_file), Path(header_to_generate_path)):
                should_recompile = True

            # source_file is /full/path/to/name.stage.in
            # source_file_basename is name.stage.in
            source_file_basename = os.path.basename(source_file)
            parts = validate_filename_and_get_parts(source_file_basename)
            if parts is None:
                continue

            shader_stage_string = parts[1]
            shader_name = parts[0] + "_" + shader_stage_string
            shader_stage = string_to_shader_stage[shader_stage_string]

            shaders_to_compile.append((source_file, shader_name, shader_stage))

    if should_recompile or args.force:
        with open(header_to_generate_path, 'w', encoding='utf-8') as generated_header_handle:
            header_source = compile_all_shaders(shaders_to_compile)
            # TODO the linker doesn't like defining the arrays in the header, and I should
            # separate this out into a c file too one day
            generated_header_handle.write(header_source)
    else:
        print(f"compile_shaders.py: Nothing to be done for {generated_header_name}")

