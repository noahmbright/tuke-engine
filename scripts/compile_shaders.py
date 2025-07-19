#!/usr/local/bin/python3
import os
import re
import sys
import subprocess
import tempfile
import argparse
from pathlib import Path
from enum import Enum, auto

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

    IN = auto()
    OUT = auto()
    VERSION = auto()
    VOID = auto()
    UNIFORM = auto()
    SAMPLER2D = auto()

    VEC2 = auto()
    VEC3 = auto()
    VEC4 = auto()
    MAT2 = auto()
    MAT3 = auto()
    MAT4 = auto()

    DIRECTIVE_VERSION = auto()
    DIRECTIVE_LOCATION = auto()
    DIRECTIVE_SET_BINDING = auto()

    TEXT = auto()

text_to_glsl_keyword = {
    "in": TokenType.IN,
    "out": TokenType.OUT,
    "version": TokenType.VERSION,
    "void": TokenType.VOID,
    "uniform": TokenType.UNIFORM,
    "sampler2D": TokenType.SAMPLER2D,

    "vec2": TokenType.VEC2,
    "vec3": TokenType.VEC3,
    "vec4": TokenType.VEC4,
    "mat2": TokenType.MAT2,
    "mat3": TokenType.MAT3,
    "mat4": TokenType.MAT4,

    "VERSION": TokenType.DIRECTIVE_VERSION,
    "LOCATION": TokenType.DIRECTIVE_LOCATION,
    "SET_BINDING": TokenType.DIRECTIVE_SET_BINDING,
}

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
            tokens.append(TokenType.POUND)
            i += 1
        elif s[i] == '{':
            if i + 1 < n and s[i + 1] == '{':
                tokens.append(TokenType.DOUBLE_L_BRACE)
                i += 2
            else:
                tokens.append(TokenType.L_BRACE)
                i += 1
        elif s[i] == '}':
            if i + 1 < n and s[i + 1] == '}':
                tokens.append(TokenType.DOUBLE_R_BRACE)
                i += 2
            else:
                tokens.append(TokenType.R_BRACE)
                i += 1
        elif s[i] == '(':
            tokens.append(TokenType.L_PAREN)
            i += 1
        elif s[i] == ')':
            tokens.append(TokenType.R_PAREN)
            i += 1
        elif s[i] == '[':
            tokens.append(TokenType.L_BRACKET)
            i += 1
        elif s[i] == ']':
            tokens.append(TokenType.R_BRACKET)
            i += 1
        elif s[i] == ';':
            tokens.append(TokenType.SEMICOLON)
            i += 1
        elif s[i] == ',':
            tokens.append(TokenType.COMMA)
            i += 1
        elif s[i] == '.':
            tokens.append(TokenType.PERIOD)
            i += 1
        elif s[i] == '=':
            tokens.append(TokenType.EQUALS)
            i += 1
        else:
            text = lex_text()
            if text in text_to_glsl_keyword:
                tokens.append(text_to_glsl_keyword[text])
            else:
                tokens.append((token_type, text))

    return tokens

vulkan_backend = {
        "name": "vulkan",
        "VERSION": "450",
        "EXTENSION": "spv"
    }

opengl_backend = {
        "name": "opengl",
        "VERSION": "410 core",
        "EXTENSION": "glsl"
    }

file_stage_to_flag = {
    "vert": "VK_SHADER_STAGE_VERTEX_BIT",
    "frag": "VK_SHADER_STAGE_FRAGMENT_BIT",
    "comp": "VK_SHADER_STAGE_COMPUTE_BIT",
}

def source_is_newer(source, target):
    if not target.exists():
        return True
    return source.stat().st_mtime > target.stat().st_mtime

# check if any of files are newer than header
# files should be shader source files. if they are newer, return true as
# a signal to recompile
def check_dir_needs_regenerated(files, header):
    needs_regenerated = False
    for file in files:
        if source_is_newer(Path(file), Path(header)):
            needs_regenerated = True
            break
    return needs_regenerated

# expect name.{vert/frag/comp}.in
def validate_filename_and_get_parts(filename):
    parts = filename.split(".")
    stage_is_valid = parts[1] in file_stage_to_flag  
    if not (len(parts) == 3 and stage_is_valid and parts[2] == "in"):
        print("Expected filename of the form {{name}}.{{vert/frag/comp}}.{{in}}")
        print("\tNot compiling file: " + filename)
        return None
    return parts

# returns a string with template-expanded glsl
def compile_shader(filepath, backend):
    with open(filepath, 'r', encoding='utf-8') as f:
        contents = f.read()

    is_vulkan = (backend["name"] == "vulkan")

    def replacer(match):
        tokens = match.group(1).split()
        directive = tokens[0]

        if directive == "LOCATION":
            if is_vulkan:
                return f"layout(location = {tokens[1]}) "
            return ""

        if directive == "SET_BINDING":
            if is_vulkan:
                return f"layout(set = {tokens[1]}, binding = {tokens[2]}) "
            return ""

        if directive in backend:
            return backend[directive]
        else:
            print(f"Failed to find key {directive} in backend {backend}")
            return match.group(0)

    return re.sub(r"\{\{\s*(.*?)\s*\}\}", replacer, contents)

# write source code to a temp file, compile that temp file into spv, and return array with bytecode
def compile_to_spirv_and_get_bytes(source_code, stage):
    if args.dump_vulkan_source:
        print(f"dumping source")
        print(source_code)

    # write glsl to file
    with tempfile.NamedTemporaryFile(suffix=f".{stage}", delete=False, mode='w', encoding='utf-8') as tmp_file:
        tmp_file.write(source_code)
        glsl_path = tmp_file.name

    # will need to read spv from other temp file
    with tempfile.NamedTemporaryFile(suffix=f".{stage}", delete=False) as tmp_file:
        spv_path = tmp_file.name

    try:
        result = subprocess.run([
            "glslangValidator",
            "-S", stage,
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

# stage is a string, "vert", etc. for opengl/vulkan flexibility
class Shader:
    def __init__(self, name, spirv, opengl_source, stage):
        self.name = name
        self.spirv = spirv
        self.stage = stage
        self.opengl_source = opengl_source

def c_multiline_string_literal(s):
    lines = s.splitlines()
    quoted_lines = ['"' + line.replace('"', '\\"') + '"' for line in lines]
    return "\n".join(quoted_lines)

def generate_shader_header(shaders):
    lines = []
    lines.append("// Generated shader header, do not edit")
    lines.append("#pragma once\n#include <stdint.h>\n#include <stddef.h>\n#include \"vulkan_base.h\"\n")

    for shader in shaders:
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
        # these assignments don't occur inside a function
        lines.append(f"constexpr const ShaderSpec {shader.name}_spec = {{")
        lines.append(f"\t.spv = {shader.name},")
        lines.append(f"\t.size = sizeof({shader.name}),")
        lines.append(f"\t.name = {shader.name}_name,")
        lines.append(f"\t.stage_flags = {stage_flags},")
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
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.realpath(__file__))
    project_root = Path(os.path.abspath(os.path.join(script_dir, "..")))
    shaders_dir = project_root / "shaders"
    # header gen dir is where the generated C code goes, for use in the engine
    header_gen_dir = project_root / "gen"

    if not os.path.isdir(shaders_dir):
        print(f"Error: Could not find shaders directory at '{shaders_dir}'")
        sys.exit(1)

    shader_subdirectories = [x[0] for x in os.walk(shaders_dir) if os.path.basename(x[0]) != "gen"]
    os.makedirs(header_gen_dir, exist_ok=True)

    # for each directory in shaders/, if any shader has been updated since last compilation, recompile entire directory
    for subdir in shader_subdirectories:
        files = [os.path.abspath(os.path.join(subdir, f)) for f in os.listdir(subdir) if os.path.isfile(os.path.join(subdir, f))]

        subdir_relative_path = str(Path(subdir).relative_to(shaders_dir)).replace("/", "_") 
        if subdir_relative_path == ".":
            subdir_relative_path = "shaders.h"
        else:
            subdir_relative_path += "_shaders.h"
        header_to_generate_path = header_gen_dir / subdir_relative_path

        if not args.force:
            if not check_dir_needs_regenerated(files, header_to_generate_path):
                print(f"Nothing to compile for {header_to_generate_path}")
                continue

        shaders = []
        for source_file in files:
            # source_file is /full/path/to/name.stage.in
            # source_file_basename is name.stage.in
            source_file_basename = os.path.basename(source_file)
            parts = validate_filename_and_get_parts(source_file_basename)
            if parts is None:
                continue

            shader_stage = parts[1]
            shader_name = parts[0] + "_" + shader_stage

            opengl_glsl = compile_shader(source_file, opengl_backend)
            vulkan_glsl = compile_shader(source_file, vulkan_backend)
            spirv = compile_to_spirv_and_get_bytes(vulkan_glsl, shader_stage)
            if spirv is None:
                continue

            shader = Shader(shader_name, spirv, opengl_glsl, shader_stage)
            shaders.append(shader)

        with open(header_to_generate_path, 'w', encoding='utf-8') as generated_header_handle:
           header_source = generate_shader_header(shaders)
           generated_header_handle.write(header_source)
