import os
import re
import sys
import subprocess
import tempfile
from pathlib import Path

script_dir = os.path.dirname(os.path.realpath(__file__))
project_root = os.path.abspath(os.path.join(script_dir, ".."))
shaders_dir = Path(project_root) / "shaders"

if not os.path.isdir(shaders_dir):
    print(f"Error: Could not find shaders directory at '{shaders_dir}'")
    sys.exit(1)

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

# expect name.vert/frag/comp.in
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

    def replacer(match):
        key = match.group(1).strip()
        if key in backend:
            return backend[key]
        else:
            print(f"Failed to find key {key} in backend {backend}")
            return match.group(0)

    return re.sub(r"\{\{\s*(\w+)\s*\}\}", replacer, contents)

def compile_to_spirv(source_code, output_path, stage):
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
            print("Compilation failed:")
            print(result.stdout)
            print(result.stderr)
            return False
        else:
            return True
    finally:
        os.remove(tmp_filename)

if __name__ == "__main__":
    shader_subdirectories = [x[0] for x in os.walk(shaders_dir) if os.path.basename(x[0]) != "gen"]

    for subdir in shader_subdirectories:
        files = [os.path.abspath(os.path.join(subdir, f)) for f in os.listdir(subdir) if os.path.isfile(os.path.join(subdir, f))]
        gen_dir = os.path.join(subdir, "gen")
        os.makedirs(gen_dir, exist_ok=True)

        for source_file in files:
            source_file_basename = os.path.basename(source_file)
            parts = validate_filename_and_get_parts(source_file_basename)

            if parts is None:
                continue

            output_file_basename = parts[0] + "." + parts[1]
            test_compiled_filepath = os.path.join(gen_dir, output_file_basename + ".glsl")
            #if not source_is_newer(Path(source_file), Path(test_compiled_filepath)):
                #continue

            output_file = os.path.join(gen_dir, output_file_basename)
            for backend in backends:
                new_shader_glsl_source = compile_shader(source_file, backend)
                output_filename = output_file_basename + "." + backend["EXTENSION"]
                output_filepath = os.path.join(gen_dir, output_filename)

                if backend["name"] == "vulkan":
                    compile_to_spirv(new_shader_glsl_source, output_filepath, parts[1])

                if backend["name"] == "opengl":
                    with open(output_filepath, 'w', encoding='utf-8') as f:
                        f.write(new_shader_glsl_source)

