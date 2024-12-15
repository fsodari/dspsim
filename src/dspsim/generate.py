"""
Generate
"""

import logging


from dataclasses import dataclass, field
from pathlib import Path
import argparse
from typing import Callable
import os
import tomllib
import glob
import numpy as np

from dataclass_wizard import JSONWizard

from dspsim.config import Parameter, ModuleConfig
from dspsim import util

from typing import Literal

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


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


@dataclass
class Config(JSONWizard):
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

        global_parameters = {
            k: Parameter(k, np.array(v))
            for k, v in dspsim_tool_config.get("parameters", {}).items()
        }
        global_includes = [
            _get_abs_path(Path(p), pyproject_path)
            for p in dspsim_tool_config.get("include_dirs", [])
        ]
        global_includes.append(util.hdl_dir())

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
        _errors = {}
        for name, _source in all_module_sources.items():
            model = tool_models.get(name, {}).copy()
            _param_cfg = model.get("parameters", {})
            model_parameters = {
                k: Parameter(k, np.array(v)) for k, v in _param_cfg.items()
            }
            model_includes = [
                _get_abs_path(i, pyproject_path) for i in model.get("include_dirs", [])
            ]
            print(f"Name: {name}, Source: {_source}")
            _errors[name] = _source
            # if name == "FifoAsync":
            #     _errors.append(1)
            all_modules[name] = ModuleConfig.from_verilator(
                _source,
                parameters=global_parameters | model_parameters,
                trace=model.get("trace", global_trace),
                include_dirs=global_includes + model_includes,
                verilator_args=global_vargs + model.get("verilator_args", []),
            )
            all_modules[name].name = name
        # raise Exception(_errors)
        return cls(
            parameters=global_parameters,
            include_dirs=global_includes,
            sources=global_sources,
            trace=global_trace,
            verilator_args=global_vargs,
            models=all_modules,
        )


def run_generate_model(pyproject_path: Path, json_tool_cfg: Path, output_dir: Path):
    """"""
    logger.debug(
        f"run_generate_model. pyproject={pyproject_path}, tool_cfg={json_tool_cfg}, output_dir={output_dir}"
    )
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
        gen_file = output_dir / f"{name}.dir/{name}.h"
        os.makedirs(gen_file.parent.absolute(), exist_ok=True)
        with open(gen_file, "w") as fp:
            # raise Exception(model.port_info())
            fp.write(render_template("model.cpp.jinja", model=model, trace=model.trace))

    from .config import _vvalue_str

    # Convert valid parameters to str values. Arrays are not allowed over command line or cmake.
    for model in config.models.values():
        model.parameters = {
            k: _vvalue_str(v.value)
            for k, v in model.parameters.items()
            if not v.value.shape
        }
    with open(json_tool_cfg, "w") as fp:
        fp.write(config.to_json(indent=4))


@dataclass
class ArgsGenerate:
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

    @classmethod
    def parse_args(cls, cli_args: list[str] = None):
        parser = cls.create_parser()
        cargs = parser.parse_args(cli_args)
        return cls(**vars(cargs))


def main(cli_args: list[str] = None):
    """"""
    args = ArgsGenerate.parse_args(cli_args)
    # create console handler and set level to debug
    log_file = args.output_dir / "generate.log"
    args.output_dir.mkdir(exist_ok=True)
    ch = logging.FileHandler(log_file, mode="w")
    ch.setLevel(logging.DEBUG)

    # create formatter
    formatter = logging.Formatter("%(levelname)s - %(message)s")

    # add formatter to ch
    ch.setFormatter(formatter)

    # add ch to logger
    logging.getLogger(__name__).addHandler(ch)

    logger.debug(f"generate.main args: {args}")

    run_generate_model(args.pyproject, args.tool_cfg, args.output_dir)


if __name__ == "__main__":
    exit(main())
