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
    parser.add_argument("--target", type=str, default="", help="Make target to build (default: all)")
    parser.add_argument("--run", type=str, default="", help="Executable to run (builds only that target)")
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
        target = args.target or args.run
        make_cmd = ["make", "-C", "build", "-j32"]
        if target:
            print(f"Building target: {target}")
            make_cmd.append(target)
        subprocess.run(make_cmd, check=True)

    if args.run != "":
        exe_path = f"./build/{args.run}"
        print(f"Running {exe_path}...")
        try:
            subprocess.run([exe_path], check=True)
        except subprocess.CalledProcessError as e:
            print(f"{exe_path} exited with code {e.returncode}")
            sys.exit(e.returncode)

if __name__ == "__main__":
    main()

