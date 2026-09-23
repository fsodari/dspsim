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
    config: str
    target: list[str]
    extra: list[str]

    @classmethod
    def parse_args(cls):
        parser = argparse.ArgumentParser()
        parser.add_argument("--configure", action="store_true", dest="configure")
        parser.add_argument("--no-build", action="store_false", dest="build")
        parser.add_argument(
            "--build-dir", type=Path, default=Path("build"), dest="build_dir"
        )
        parser.add_argument("--config", type=str, default="Debug", dest="config")
        parser.add_argument(
            "--target", type=str, action="append", default=["tests"], dest="target"
        )
        args, extra = parser.parse_known_args()
        return cls(
            configure=args.configure,
            build=args.build,
            build_dir=args.build_dir,
            config=args.config,
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
        f"-DCMAKE_BUILD_TYPE={args.config}",
        "-DSKBUILD_PROJECT_NAME=dspsim",
        "-DSKBUILD_PROJECT_VERSION=0.0.0",
        f"-DCMAKE_PREFIX_PATH={sysconfig.get_path('purelib')}",
    ]
    build_cmd = ["cmake", "--build", args.build_dir]
    for target in args.target:
        build_cmd.extend(["--target", target])
    if sys.platform == "win32":
        test_dir = args.build_dir / "tests" / "cpp" / args.config
    else:
        test_dir = args.build_dir / "tests" / "cpp"

    if args.configure:
        subprocess.run(configure_cmd, check=True)
    if args.build:
        subprocess.run(build_cmd, check=True)

    for target in args.target:
        test_exe = test_dir / target

        test_cmd = [test_exe.as_posix()]
        test_cmd.extend(args.extra)
        # No need to check test command since we want to see its output on failure
        subprocess.run(test_cmd, check=False)


if __name__ == "__main__":
    main()
