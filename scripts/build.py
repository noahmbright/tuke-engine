#!/usr/bin/env python3
import subprocess
import sys
import argparse

def main():
    parser = argparse.ArgumentParser(description="Build, compile shaders, and run executables.")
    parser.add_argument("--no-shaders", action="store_true", help="Skip shader compilation")
    parser.add_argument("--force-shaders", action="store_true", help="Force shader compilation")
    parser.add_argument("--no-build", action="store_true", help="Skip building")
    parser.add_argument("--run", type=str, default="", help="Executable to run from ./build/")
    args = parser.parse_args()

    if not args.no_shaders:
        invocations = ["python3", "scripts/compile_shaders.py"]
        if args.force_shaders:
            invocations.append("--force")
        subprocess.run(invocations, check=True)

    if not args.no_build:
        subprocess.run(["make", "-C", "build"], check=True)

    if args.run != "":
        exe_path = f"./build/{args.run}"
        print(f"🚀 Running {exe_path}...")
        subprocess.run([exe_path], check=True)

if __name__ == "__main__":
    main()

