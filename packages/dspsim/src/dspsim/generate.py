"""
Generate 
"""

from dataclasses import dataclass, field, fields
from pathlib import Path
import argparse
from typing import Callable
import sys
import os
import tomllib
import glob


# In order to run this as a script. I have to define all the config settings here instead of config.py
# Maybe I should avoid running this script during the framework build? Build the library as a seperate project.
@dataclass
class ConfigModule:
    source: Path
    parameters: dict[str, int | str] = field(default_factory=dict)
    include_dirs: list[Path] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: dict[str, dict[str]]):
        """"""


@dataclass
class Config:
    # Build config
    library_type: str = "static"

    # Global parameters
    parameters: dict[str, int | str] = field(default_factory=dict)

    # Global include dirs
    include_dirs: list[Path] = field(default_factory=list)

    # Sources to use. Will build modules for all sources with default/global settings.
    sources: list[str] = field(default_factory=list)

    # Module configurations. Specify different configurations, non-default options, or overrides.
    modules: dict[str, ConfigModule] = field(default_factory=dict)

    @classmethod
    def from_pyproject(cls, pyproject_path: Path = Path("pyproject.toml")):
        """Read in the config from the pyproject.toml file."""
        with open(pyproject_path, "rb") as fp:
            pyproject = tomllib.load(fp)
        tool_config: dict = pyproject["tool"]["dspsim"]

        global_parameters = tool_config.get("parameters", {})
        global_include_dirs = tool_config.get("include_dirs", [])

        # if sources were given, populate the modules with these. Glob paths.
        sources = [
            filename
            for source in tool_config.get("sources", [])
            for filename in glob.glob(source)
        ]

        # Read in modules from sources.
        source_modules = {
            m: ConfigModule(Path(source), global_parameters, global_include_dirs)
            for m, source in sources
        }

        # Read in extra config.
        config_modules = {
            m: ConfigModule(**cfg) for m, cfg in tool_config.get("modules", {})
        }

        #
        for sname, smod in source_modules:
            # Override parameters,
            if sname in config_modules.keys():
                #
                source_modules[sname].parameters = config_modules[sname].parameters
                source_modules[sname].include_dirs = config_modules[sname].include_dirs
        # for mod, cfg in config_modules:
        #     source_modules[]

        return cls(global_parameters, global_include_dirs, sources, modules)


cmake_env_variables = [
    "DSPSIM_LIB_TYPE",
    "DSPSIM_PARAMETERS",
    "DSPSIM_INCLUDE_DIRS",
    "DSPSIM_",
]


@dataclass
class ArgsGenerate:
    source: Path
    include_dirs: list[Path]
    func: Callable = field(repr=False)

    @classmethod
    def create_parser(cls, subparser: argparse.ArgumentParser = None):
        """"""
        help_str = "Generate code from templates."
        if subparser is None:
            parser = argparse.ArgumentParser("dspsim-generate")
        else:
            parser = subparser.add_parser("generate", help=help_str)
        parser.add_argument(
            "source", type=Path, help="HDL Source to generate bindings for."
        )
        parser.add_argument(
            "include_dirs", nargs="*", type=Path, help="Additional HDL Include Dirs."
        )
        parser.set_defaults(func=ArgsGenerate.parse_argparse_args)
        return parser

    @classmethod
    def parse_argparse_args(cls, args: dict[str]):
        """"""
        field_names = [f.name for f in fields(cls)]
        filtered_args = {k: a for k, a in args if k in field_names}
        return cls(**filtered_args)

    @classmethod
    def parse_args(cls, cli_args: list[str] = None):
        parser = cls.create_parser()
        return cls(**vars(parser.parse_args(cli_args)))

    def exec(self):
        """"""
        print(f"Running generate_func: {self}")


def main(cli_args: list[str] = None):
    """"""
    args = ArgsGenerate.parse_args(cli_args)
    print(args)


if __name__ == "__main__":
    exit(main())
