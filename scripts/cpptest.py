#!/usr/bin/env python3

import argparse
import subprocess
from dataclasses import dataclass
from pathlib import Path
import sys

@dataclass
class Args:
    configure: bool
    build: bool
    extra: list[str]

    @classmethod
    def parse_args(cls):
        parser = argparse.ArgumentParser()
        parser.add_argument("--configure", action="store_true", dest="configure")
        parser.add_argument("--no-build", action="store_false", dest="build")
        # args = parser.parse_args()
        args, extra = parser.parse_known_args()
        return cls(configure=args.configure, build=args.build, extra=extra)


def main():
    """Run the C++ test using subprocess."""

    args = Args.parse_args()

    build_dir = Path("build")
    if sys.platform == "win32":
        build_preset = "cpp-testing-windows"
        configure_cmd = ["cmake", "-S", ".", "--preset", build_preset]
        build_cmd = ["cmake", "--build", "--preset", build_preset]
        test_dir = build_dir / "tests" / "cpp" / "Debug"
    else:
        build_preset = "cpp-testing-linux"
        configure_cmd = ["cmake", "-S", ".", "--preset", build_preset]
        build_cmd = ["cmake", "--build", "--preset", build_preset]
        test_dir = build_dir / "tests" / "cpp"

    if args.configure:
        subprocess.run(configure_cmd, check=True)
    if args.build:
        subprocess.run(build_cmd, check=True)

    
    
    test_exe = test_dir / "tests"

    test_cmd = [test_exe.as_posix()]
    test_cmd.extend(args.extra)
    # No need to check test command since we want to see its output on failure
    subprocess.run(test_cmd, check=False)


if __name__ == "__main__":
    main()
