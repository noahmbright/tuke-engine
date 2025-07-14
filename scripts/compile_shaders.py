#!/usr/local/bin/python3
import os
import re
import sys
import subprocess
import tempfile
import argparse
from pathlib import Path

# TODO reorganize naming
# TODO output a final struct at the end that I can cache all at once

# wanted to try validating linkage at compile time, had this idea
# but there doesn't seem to be a cli tool validating correct linkage 
# keeping in case someone invents it
   # output_files_to_link = {}
   # if parts[0] in output_files_to_link:
   #     if output_filepath in output_files_to_link[parts[0]]:
   #         print(f"repeat compilation of {output_filepath}")
   #     else:
   #         output_files_to_link[parts[0]].append(output_filepath) 
   # else:
   #     output_files_to_link[parts[0]] = [output_filepath]

backends = [
    {
        "name": "vulkan",
        "VERSION": "450",
        "EXTENSION": "spv"
    },
    {
        "name": "opengl",
        "VERSION": "410 core",
        "EXTENSION": "glsl"
    }
]

def source_is_newer(source, target):
    if not target.exists():
        return True
    return source.stat().st_mtime > target.stat().st_mtime

def check_dir_needs_regenerated(files, shader_gen_dir):
    needs_regenerated = False
    for file in files:
        source_file_basename = os.path.basename(file)
        parts = validate_filename_and_get_parts(source_file_basename)
        if parts is None:
            continue

        shader_stage = parts[1]
        output_file_basename = parts[0] + "." + shader_stage
        test_compiled_filepath = os.path.join(shader_gen_dir, output_file_basename + ".glsl")
        if source_is_newer(Path(file), Path(test_compiled_filepath)):
            needs_regenerated = True
            break
    return needs_regenerated

# expect name.{vert/frag/comp}.in
def validate_filename_and_get_parts(filename):
    parts = filename.split(".")
    if len(parts)!= 3 or not (parts[1] == "vert" or parts[1] == "frag" or parts[1] == "comp") or parts[2] != "in":
        print("Expected filename of the form {{name}}.{{vert/frag/comp}}.{{in}}")
        print("\tNot compiling file: " + filename)
        return None
    return parts

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

def compile_to_spirv(source_code, output_path, stage):
    if args.dump_vulkan_source:
        print(f"dumping source for {output_path}")
        print(source_code)

    with tempfile.NamedTemporaryFile(suffix=f".{stage}", delete=False, mode='w', encoding='utf-8') as tmp_file:
        tmp_file.write(source_code)
        tmp_filename = tmp_file.name

    try:
        result = subprocess.run([
            "glslangValidator",
            "-S", stage,
            "-o", output_path,
            "-V", tmp_filename
        ], capture_output=True, text=True)

        if result.returncode != 0:
            print("Compilation failed. Source dump:")
            print(source_code)
            print(result.stdout)
            print(result.stderr)
            return False
        else:
            return True
    finally:
        os.remove(tmp_filename)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dump-vulkan-source",
        action="store_true",
        help="Print vulkan source when compiling to glsl 450"
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
        # shader gen dir is where the generated glsl goes
        shader_gen_dir = os.path.join(subdir, "gen")
        os.makedirs(shader_gen_dir, exist_ok=True)

        if not check_dir_needs_regenerated(files, shader_gen_dir):
            continue

        subdir_relative_path = str(Path(subdir).relative_to(shaders_dir)).replace("/", "_") 
        if subdir_relative_path == ".":
            subdir_relative_path = "shaders.h"
        else:
            subdir_relative_path += "_shaders.h"
        header_to_generate_path = header_gen_dir / subdir_relative_path

        with open(header_to_generate_path, 'w', encoding='utf-8') as generated_header_handle:
            generated_header_handle.write("// Generated shader header, do not edit\n")
            generated_header_handle.write("#pragma once\n#include <stdint.h>\n#include <stddef.h>\n#include \"vulkan_base.h\"\n\n")

            for source_file in files:
                source_file_basename = os.path.basename(source_file)
                parts = validate_filename_and_get_parts(source_file_basename)
                if parts is None:
                    continue

                shader_stage = parts[1]
                output_file_basename = parts[0] + "." + shader_stage
                output_file = os.path.join(shader_gen_dir, output_file_basename)

                for backend in backends:
                    new_shader_glsl_source = compile_shader(source_file, backend)
                    output_filename = output_file_basename + "." + backend["EXTENSION"]
                    output_filepath = os.path.join(shader_gen_dir, output_filename)

                    if backend["name"] == "vulkan":
                        compile_to_spirv(new_shader_glsl_source, output_filepath, shader_stage)

                        with open(output_filepath, "rb") as spirv_f:
                            spirv = spirv_f.read()
                        if len(spirv) % 4 != 0:
                            print(f"\tThe length of {output_filepath} is {len(spirv)}, not a multiple of 4")
                            print(f"\tNot writing {output_filepath} to c header")
                            continue

                        u32_words = [int.from_bytes(spirv[i:i+4], "little") for i in range(0, len(spirv), 4)]
                        shader_array_name = output_filename.replace(".", "_")

                        stage_flags = "VK_SHADER_STAGE_VERTEX_BIT"
                        if parts[1] == "frag":
                            stage_flags = "VK_SHADER_STAGE_FRAGMENT_BIT"

                        generated_header_handle.write(f"static const uint32_t {shader_array_name}[] = {{\n")
                        for i in range(0, len(u32_words), 4):
                            chunk = ", ".join(f"0x{w:08x}" for w in u32_words[i:i+4])
                            generated_header_handle.write(f"    {chunk},\n")
                        generated_header_handle.write("};\n")
                        generated_header_handle.write(f"static const size_t {shader_array_name}_size = sizeof({shader_array_name});\n")
                        generated_header_handle.write(f"static constexpr const char* {shader_array_name}_name = \"{shader_array_name}\";\n")

                        # need to output a constexpr const struct like this with the .initialization because
                        # these assignments don't occur inside a function
                        generated_header_handle.write(f"constexpr const ShaderSpec {shader_array_name}_spec = {{\n")
                        generated_header_handle.write(f"\t.spv = {shader_array_name},\n")
                        generated_header_handle.write(f"\t.size = sizeof({shader_array_name}),\n")
                        generated_header_handle.write(f"\t.name = {shader_array_name}_name,\n")
                        generated_header_handle.write(f"\t.stage_flags = {stage_flags},\n")
                        generated_header_handle.write("};\n\n")

                    if backend["name"] == "opengl":
                        with open(output_filepath, 'w', encoding='utf-8') as f:
                            f.write(new_shader_glsl_source)

