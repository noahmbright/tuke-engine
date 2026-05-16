#!/usr/bin/env python3
import subprocess
import sys
import argparse
import os

REFLECTOR_PATH = "./build/reflector"

def build_reflector():
    print("Building reflector...")
    subprocess.run(["make", "-C", "build", "reflector"], check=True)

def main():
    parser = argparse.ArgumentParser(description="Build, compile shaders, and run executables.")
    parser.add_argument("--no-shaders", action="store_true", help="Skip shader compilation")
    parser.add_argument("--force-shaders", action="store_true", help="Force shader compilation")
    parser.add_argument("--no-build", action="store_true", help="Skip building")
    parser.add_argument("--run", type=str, default="", help="Executable to run from ./build/")
    args = parser.parse_args()

    if not os.path.isdir("build"):
        print("build/ not found, running cmake...")
        subprocess.run(["cmake", "-B", "build", "-S", "."], check=True)

    build_reflector()

    if not args.no_shaders:
        invocations = [REFLECTOR_PATH]
        if args.force_shaders:
            invocations.append("--force-shaders")

        try:
            subprocess.run(invocations, check=True)
        except subprocess.CalledProcessError:
            print("Reflector failed. Stopping")
            exit(1)

    if not args.no_build:
        subprocess.run(["make", "-C", "build"], check=True)

    if args.run != "":
        exe_path = f"./build/{args.run}"
        print(f"Running {exe_path}...")
        subprocess.run([exe_path], check=True)

if __name__ == "__main__":
    main()

