"""
Generate 
"""

from dataclasses import dataclass, field, fields, replace
from pathlib import Path
import argparse
from typing import Callable
import sys
import os
import tomllib
import json
import shutil
import glob
import numpy as np
import subprocess
from tempfile import TemporaryDirectory
from typing import Literal, TypeAlias

from dataclass_wizard import TOMLWizard, JSONWizard
from functools import cache

from dspsim.config import Parameter, Port, ModuleConfig
from dspsim import util


def _get_abs_path(source: Path, pyproject_path: Path) -> Path:
    if Path(source).is_absolute():
        # An absolute path is given, do nothing.
        return Path(source)
    else:
        # Add relative to pyproject.toml parent if a relative path was given.
        return Path(pyproject_path.parent / source).absolute()


def _find_source(
    source: Path,
    global_sources: list[Path],
    include_dirs: list[Path],
    pyproject_path: Path,
) -> Path:
    """
    If a source is not an absolute path, find it using the configuration.
    Search Order:
    - check if abs.
    - check in global sources list.
    - check in include_dirs (including dspsim/hdl)
    - check relative to pyproject.toml directory.
    """

    if source.is_absolute():
        return source

    # Check global sources
    for s in global_sources:
        if source.name == s.name:
            return s
    # Check global sources stem name
    for s in global_sources:
        if source.stem == s.stem:
            return s
    # Check include_dirs
    for idir in include_dirs:
        if Path(idir / source).exists():
            return Path(idir / source)
    # Check include_dirs stem name?
    for idir in include_dirs:
        for src in idir.iterdir():
            if source.stem == src.stem:
                return src
    # Check relative to pyproject.toml
    if Path(pyproject_path.parent / source).exists():
        return Path(pyproject_path.parent / source)
    # Check stem relative to pyproject.toml
    for src in pyproject_path.parent.iterdir():
        if source.stem == src.stem:
            return src

    raise Exception(source, global_sources, include_dirs, pyproject_path)


@dataclass
class Config(JSONWizard):
    # Build config
    library_type: Literal["static", "shared"] = "static"

    # Global parameters
    parameters: dict[str, Parameter] = field(default_factory=dict)

    # Global include dirs
    include_dirs: list[Path] = field(
        default_factory=lambda: [Path(__file__).parent / "hdl"]
    )

    # Sources to use. Will build modules for all sources with default/global settings.
    sources: list[Path] = field(default_factory=list)

    # Tracing. Options vcd, fst, None
    trace: Literal[None, "vcd", "fst", "notrace"] = None

    verilator_args: list = field(default_factory=list)

    # Module configurations. Specify different configurations, non-default options, or overrides.
    models: dict[str, ModuleConfig] = field(default_factory=dict)

    @classmethod
    def from_pyproject(cls, pyproject_path: Path = Path("pyproject.toml")):
        """Read in the complete configuration for all models."""
        pyproject_path = pyproject_path.absolute()

        with open(pyproject_path, "rb") as fp:
            pyproject = tomllib.load(fp)
        dspsim_tool_config: dict = pyproject["tool"]["dspsim"]

        library_type = dspsim_tool_config.get("library_type", "static")
        global_parameters = {
            k: Parameter(k, np.array(v))
            for k, v in dspsim_tool_config.get("parameters", {}).items()
        }
        global_includes = [
            Path(p) for p in dspsim_tool_config.get("include_dirs", [util.hdl_dir()])
        ]

        global_trace = dspsim_tool_config.get("trace", None)
        global_vargs = dspsim_tool_config.get("verilator_args", [])

        # Find all default sources
        global_sources: set[Path] = set()
        for source in dspsim_tool_config.get("sources", []):
            # Glob every source, and add all options to the set.
            # fs.append(source)
            abp = _get_abs_path(
                Path(source),
                pyproject_path,
            )
            for filename in glob.glob(str(abp)):
                # fs.append(filename)
                # Add to the set.
                found = _find_source(
                    Path(filename),
                    global_sources,
                    global_includes,
                    pyproject_path,
                )
                global_sources.add(found)
        tool_models: dict = dspsim_tool_config.get("models", {})
        default_module_sources = {source.stem: source for source in global_sources}
        extra_module_sources = {
            model_name: _get_abs_path(
                _find_source(
                    Path(model.get("source", model_name)),
                    global_sources,
                    global_includes + model.get("include_dirs", []),
                    pyproject_path,
                ),
                pyproject_path,
            )
            for model_name, model in tool_models.items()
        }
        all_module_sources = default_module_sources | extra_module_sources

        all_modules: dict[str, ModuleConfig] = {}
        for name, source in all_module_sources.items():
            model = tool_models.get(name, {}).copy()
            _param_cfg = model.get("parameters", {})
            model_parameters = {
                k: Parameter(k, np.array(v)) for k, v in _param_cfg.items()
            }
            model_includes = [
                _get_abs_path(i, pyproject_path) for i in model.get("include_dirs", [])
            ]
            all_modules[name] = ModuleConfig.from_verilator(
                source,
                parameters=global_parameters | model_parameters,
                trace=model.get("trace", global_trace),
                include_dirs=global_includes + model_includes,
                verilator_args=global_vargs + model.get("verilator_args", []),
            )
            all_modules[name].name = name
        return cls(
            library_type=library_type,
            parameters=global_parameters,
            include_dirs=global_includes,
            sources=global_sources,
            trace=global_trace,
            verilator_args=global_vargs,
            models=all_modules,
        )
        # for mname, m in dspsim_tool_config["models"].items():
        #     m["parameters"] = {
        #         k: Parameter(k, np.array(v)) for k, v in m["parameters"].items()
        #     }
        #     m = ModuleConfig()
        # tool_config = cls(**dspsim_tool_config)
        # print(f"{tool_config}")

        # # fix relative include dirs.
        # tool_config.include_dirs = [
        #     _get_abs_path(idir, pyproject_path) for idir in tool_config.include_dirs
        # ]
        # # Collect all sources from sources field. globbed.
        # # Relative to pyproject.toml path if not absolute.
        # found_sources: set[Path] = set()
        # for source in tool_config.sources:
        #     # Glob every source, and add all options to the set.
        #     for filename in glob.glob(str(source)):
        #         # Add to the set.
        #         found_sources.add(_get_abs_path(Path(filename), pyproject_path))

        # # Add any extra sources specified in the models config.
        # for model_name, model in tool_config.models.items():
        #     params =
        #     model = ModuleConfig(
        #         model.get("name", None),
        #         model.get("source", None),
        #         params,
        #     )
        #     if model.get("source", None):
        #         source_name = model.source
        #     else:
        #         source_name = Path(model_name)
        #     found = _find_source(
        #         source_name,
        #         found_sources,
        #         tool_config.include_dirs,
        #         pyproject_path,
        #     )
        #     model.source = found
        #     found_sources.add(found)

        # tool_config.sources = found_sources
        # # Get all of the default parameters for each source.
        # # Set up default models.
        # default_models: dict[Path, ModuleConfig] = {}
        # for source in tool_config.sources:
        #     default_models[source] = ModuleConfig.from_verilator(
        #         source,
        #         parameters=tool_config.parameters,
        #         verilator_args=tool_config.verilator_args,
        #     )
        # default_source_params = {
        #     k: v.parameters.copy() for k, v in default_models.items()
        # }
        # final_models: dict[str, ModuleConfig] = default_models.copy()
        # for name, model in tool_config.models.items():
        #     params = default_source_params.get(model.source, {}).copy()
        #     # Overrides.
        #     for param_name in params:
        #         if param_name in model.parameters:
        #             params[param_name] = model.parameters[param_name]

        #     # Add new model
        #     final_models[name] = ModuleConfig.from_verilator(
        #         model.source, parameters=params.copy()
        #     )

        # tool_config.models = final_models

        # return tool_config


def run_generate_model(pyproject_path: Path, json_tool_cfg: Path, output_dir: Path):
    """"""
    from dspsim.util import render_template

    config = Config.from_pyproject(pyproject_path)

    lib_odir = output_dir
    os.makedirs(output_dir, exist_ok=True)
    with open(lib_odir / f"{lib_odir.stem}.cpp", "w") as fp:
        fp.write(
            render_template(
                "module_library.cpp.jinja",
                models=config.models,
                libname=f"{lib_odir.stem}",
            )
        )
    for name, model in config.models.items():
        prefix = f"V{name}"
        gen_file = output_dir / f"{name}.dir/{name}.h"
        os.makedirs(gen_file.parent.absolute(), exist_ok=True)
        with open(gen_file, "w") as fp:
            fp.write(render_template("model.cpp.jinja", model=model, trace=model.trace))

    # Remove array-like parameters.
    # xparameters = {k: v for k, v in config.parameters.items() if not v.value.shape}
    # config.parameters = xparameters
    from .config import _vvalue_str

    # fmt_param = lambda v:
    for model in config.models.values():
        model.parameters = {
            k: _vvalue_str(v.value)
            for k, v in model.parameters.items()
            if not v.value.shape
        }
    # raise Exception(f"{config.parameters}")
    with open(json_tool_cfg, "w") as fp:
        x = config.to_json(indent=4)
        # # raise Exception(f"{x[]}")
        # for model in x.get("models", {}).values():
        #     for n, param in model.get("parameters", {}).items():
        #         param["value"] = fmt_param(param["value"])
        # raise Exception(f"{x}")
        fp.write(x)


@dataclass
class ArgsGenerate:
    # generate_models: tuple[Path, Path, Path]
    pyproject: Path
    tool_cfg: Path
    output_dir: Path
    func: Callable = None

    @classmethod
    def create_parser(cls, subparser: argparse.ArgumentParser = None):
        """"""
        help_str = "Generate code from templates."
        if subparser is None:
            parser = argparse.ArgumentParser("dspsim-generate")
        else:
            parser = subparser.add_parser("generate", help=help_str)
        parser.add_argument(
            "--pyproject",
            type=Path,
            default=Path("pyproject.toml"),
            help="pyproject.toml config.",
        )
        parser.add_argument(
            "--tool-cfg",
            type=Path,
            help="dspsim json tool config.",
        )
        parser.add_argument(
            "--output-dir", type=Path, help="Output dir for generated models."
        )
        parser.set_defaults(func=ArgsGenerate.parse_args)
        return parser

    # @classmethod
    # def parse_argparse_args(cls, args: dict[str]):
    #     """"""
    #     field_names = [f.name for f in fields(cls)]
    #     filtered_args = {k: a for k, a in args.items() if k in field_names}
    #     return cls(**filtered_args)

    @classmethod
    def parse_args(cls, cli_args: list[str] = None):
        parser = cls.create_parser()
        cargs = parser.parse_args(cli_args)
        # raise Exception(f"{cargs}")
        return cls(**vars(cargs))

    def exec(self):
        """"""
        run_generate_model(self.pyproject, self.tool_cfg, self.output_dir)


def main(cli_args: list[str] = None):
    """"""
    args = ArgsGenerate.parse_args(cli_args)
    args.exec()


def foo():
    raise Exception("Foo")


if __name__ == "__main__":
    exit(main())
