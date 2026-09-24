#!/usr/bin/env python3

import argparse
import subprocess
import sys
import sysconfig
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Args:
    configure: bool
    build: bool
    build_dir: Path
    benchmarks: list[str]
    target: str
    extra: list[str]

    @classmethod
    def parse_args(cls):
        parser = argparse.ArgumentParser()
        parser.add_argument("--configure", action="store_true", dest="configure")
        parser.add_argument("--no-build", action="store_false", dest="build")
        parser.add_argument(
            "--build-dir",
            type=Path,
            default=Path("build"),
            dest="build_dir",
        )
        parser.add_argument(
            "-b",
            "--benchmark",
            action="append",
            type=str,
            default=[],
            dest="benchmarks",
        )
        parser.add_argument(
            "--target",
            type=str,
            default="benchmarks",
            dest="target",
        )
        # args = parser.parse_args()
        args, extra = parser.parse_known_args()
        return cls(
            configure=args.configure,
            build=args.build,
            build_dir=args.build_dir,
            benchmarks=args.benchmarks,
            target=args.target,
            extra=extra,
        )


def main():
    """Run the C++ test using subprocess."""

    args = Args.parse_args()

    configure_cmd = [
        "cmake",
        "-S",
        ".",
        "-B",
        args.build_dir,
        "-DCMAKE_BUILD_TYPE=Release",
        "-DSKBUILD_PROJECT_NAME=dspsim",
        "-DSKBUILD_PROJECT_VERSION=0.0.0",
        f"-DCMAKE_PREFIX_PATH={sysconfig.get_path('purelib')}",
    ]
    build_cmd = [
        "cmake",
        "--build",
        args.build_dir,
        "--target",
        args.target,
        "--config",
        "Release",
    ]
    exe_dir = args.build_dir / "benchmarks"
    if sys.platform == "win32":
        exe_dir = exe_dir / "Release"

    if args.configure:
        subprocess.run(configure_cmd, check=True)
    if args.build:
        subprocess.run(build_cmd, check=True)

    if len(args.benchmarks) == 0:
        # Find all exes in the exe dir and run them.
        suffix = ".exe" if sys.platform == "win32" else ""
        for test_exe in exe_dir.glob("*"):
            if test_exe.is_file() and test_exe.suffix == suffix:
                args.benchmarks.append(test_exe.name)

    for benchmark in args.benchmarks:
        test_exe = exe_dir / benchmark
        test_cmd = [test_exe.as_posix()]
        test_cmd.extend(args.extra)

        print(f"Running benchmark: {benchmark}")
        # No need to check test command since we want to see its output on failure
        subprocess.run(test_cmd, check=False)


if __name__ == "__main__":
    main()
