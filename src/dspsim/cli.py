"""Command-line interface for the dspsim framework."""

import argparse
from dataclasses import dataclass

from . import cmake_dir


@dataclass
class Args:
    """Command-line arguments for the dspsim framework."""

    cmake_dir: bool

    @classmethod
    def parse_args(cls):
        parser = argparse.ArgumentParser(
            description="Command-line interface for the dspsim framework."
        )
        parser.add_argument(
            "--cmake_dir", action="store_true", help="Specify the CMake directory."
        )
        args = parser.parse_args()
        return cls(cmake_dir=args.cmake_dir)


def main():
    args = Args.parse_args()
    if args.cmake_dir:
        print(cmake_dir())
