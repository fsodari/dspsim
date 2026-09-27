"""Command-line interface for the dspsim framework."""

import argparse
from dataclasses import dataclass

import dspsim


@dataclass
class Args:
    """Command-line arguments for the dspsim framework."""

    cmake_dir: bool
    include_dir: bool

    @classmethod
    def parse_args(cls):
        parser = argparse.ArgumentParser(
            description="Command-line interface for the dspsim framework."
        )
        parser.add_argument(
            "--cmake_dir", action="store_true", help="Specify the CMake directory."
        )
        parser.add_argument(
            "--include_dir", action="store_true", help="Specify the include directory."
        )
        parser.add_argument("--version", action="version", version=dspsim.__version__)
        args = parser.parse_args()
        return cls(cmake_dir=args.cmake_dir, include_dir=args.include_dir)


def main():
    args = Args.parse_args()
    if args.cmake_dir:
        print(dspsim.cmake_dir())
    if args.include_dir:
        print(dspsim.include_dir())
