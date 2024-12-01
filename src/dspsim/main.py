"""
Main Command-line app interface "dspsim"
"""

from dataclasses import dataclass, field
from pathlib import Path
import argparse
from typing import Callable
from dspsim.generate import ArgsGenerate


@dataclass
class Args:
    cmake_dir: bool
    include_dir: bool
    hdl_dir: bool
    func: Callable

    @classmethod
    def create_parser(cls):
        """"""
        from . import __version__

        parser = argparse.ArgumentParser("dspsim")
        parser.add_argument(
            "--version",
            action="version",
            version=__version__,
            help="Print the package version.",
        )
        parser.add_argument(
            "--cmake-dir", action="store_true", help="Print the cmake config location."
        )
        parser.add_argument(
            "--include-dir",
            action="store_true",
            help="Print the C++ include directory.",
        )
        parser.add_argument(
            "--hdl-dir", action="store_true", help="Print the HDL directory."
        )
        parser.set_defaults(func=Args)
        subparsers = parser.add_subparsers()
        generate_parser = ArgsGenerate.create_parser(subparsers)

        return parser

    @classmethod
    def parse_args(cls, cli_args: list[str] = None):
        parser = Args.create_parser()
        parsed_args = parser.parse_args(cli_args)
        args: Args = parsed_args.func(**vars(parsed_args))
        return args

    def exec(self):
        """"""
        from .util import cmake_dir, include_dir, hdl_dir

        print(f"Running main_func: {self}")
        if self.cmake_dir:
            print(cmake_dir())
        if self.include_dir:
            print(include_dir())
        if self.hdl_dir:
            print(hdl_dir())


def main(cli_args: list[str] = None):

    args = Args.parse_args(cli_args)
    args.exec()


if __name__ == "__main__":
    exit(main())
