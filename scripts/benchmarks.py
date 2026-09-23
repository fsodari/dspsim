#!/usr/bin/env python3

import argparse
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Args:
    configure: bool
    build: bool
    target: list[str]
    extra: list[str]

    @classmethod
    def parse_args(cls):
        parser = argparse.ArgumentParser()
        parser.add_argument("--configure", action="store_true", dest="configure")
        parser.add_argument("--no-build", action="store_false", dest="build")
        parser.add_argument(
            "--target",
            action="append",
            type=str,
            default=[],
            dest="target",
        )
        # args = parser.parse_args()
        args, extra = parser.parse_known_args()
        return cls(
            configure=args.configure, build=args.build, target=args.target, extra=extra
        )


def main():
    """Run the C++ test using subprocess."""

    args = Args.parse_args()

    build_dir = Path("build")
    if sys.platform == "win32":
        build_preset = "cpp-benchmarks-windows"
        configure_cmd = ["cmake", "-S", ".", "--preset", build_preset]
        build_cmd = ["cmake", "--build", "--preset", build_preset]
        test_dir = build_dir / "tests" / "cpp" / "Debug"
    else:
        preset = "cpp-benchmarks-linux"
        configure_cmd = ["cmake", "-S", ".", "--preset", preset]
        build_cmd = ["cmake", "--build", "--preset", preset]
        for target in args.target:
            build_cmd.extend(["--target", target])
        test_dir = build_dir / "benchmarks"

    if args.configure:
        subprocess.run(configure_cmd, check=True)
    if args.build:
        subprocess.run(build_cmd, check=True)

    for target in args.target:
        test_exe = test_dir / target
        test_cmd = [test_exe.as_posix()]
        test_cmd.extend(args.extra)

        print(f"Running benchmark: {target}")
        # No need to check test command since we want to see its output on failure
        subprocess.run(test_cmd, check=False)


if __name__ == "__main__":
    main()
