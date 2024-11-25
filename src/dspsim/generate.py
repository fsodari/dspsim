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

# from pydantic import BaseModel, field_validator

# from pydantic.dataclasses import dataclass
import subprocess
from tempfile import TemporaryDirectory
from typing import Literal, TypeAlias

from dataclass_wizard import TOMLWizard, JSONWizard


def verilator(args: list[str]):
    """"""
    if "VERILATOR_ROOT" in os.environ:
        verilator_bin = Path(os.environ["VERILATOR_ROOT"]) / "bin/verilator"
    else:
        # Assume it's on the path.
        verilator_bin = "verilator"
    cmd = [verilator_bin, "--quiet"] + args
    subprocess.run(cmd)


def read_tree(tree_file: Path) -> dict:
    """Read"""
    with open(tree_file, "r") as fp:
        return json.load(fp)


def write_tree(tree_file: Path, content: dict):
    """"""
    with open(tree_file, "w") as fp:
        json.dump(content, fp)


def generate_tree(
    source: Path, output_file: Path = None, *, verilator_args: list[str] = []
) -> dict:
    """"""
    args = [source, "--json-only"]
    args.extend(verilator_args)
    with TemporaryDirectory() as _tmpdir:
        tmpdir = Path(_tmpdir)

        args.extend(["--Mdir", tmpdir])
        verilator(args)

        tmp_file = tmpdir / f"V{source.stem}.tree.json"

        content = read_tree(tmp_file)
        # Copy file if requested.
        if output_file:
            shutil.copy(tmp_file, output_file)

    return content


def _int_from_str(s: str) -> int:
    """"""

    def sign_extend(value, bits):
        sign_bit = 1 << (bits - 1)
        return (value & (sign_bit - 1)) - (value & sign_bit)

    fmt, val = s.split("h", maxsplit=1)
    # is_signed = "s" in fmt
    nbits = int(fmt.split("'")[0])

    # print(f"Width: {bits}, {val}, {is_signed}")
    result = int(val, base=16)
    return sign_extend(result, nbits)
    # base_str, val = _rest.split("s", maxsplit=1)


_type_names = {
    "logic": _int_from_str,
    "int": _int_from_str,
    "real": float,
    "string": lambda x: str(x).replace("\\", "").replace('"', "").replace("'", ""),
}


def _parse_value(valuep, typetable):
    """Parse parameter value using typetable."""
    return _type_names[typetable[valuep["dtypep"]]["name"]](valuep["name"])


def _parse_param_value(valuep, typetable):
    """Recursively read in parameter definition to build multi-dimensional arrays."""
    if "initsp" in valuep:
        return [_parse_param_value(i["valuep"][0], typetable) for i in valuep["initsp"]]

    return _parse_value(valuep, typetable)


Parameter: TypeAlias = int | float | str | list


def read_parameters(json_tree: dict) -> dict[str, Parameter]:
    """"""
    modulesp = json_tree["modulesp"][0]
    typetable = {t["addr"]: t for t in json_tree["miscsp"][0]["typesp"]}
    stmtsp: list[dict[str]] = modulesp["stmtsp"]

    # Read in parameters and parse the value.
    return {
        entry["name"]: _parse_param_value(entry["valuep"][0], typetable)
        for entry in stmtsp
        if entry.get("isGParam", False)
    }


def _range_to_int(r: str) -> int:
    """"""
    ul = [int(x) for x in r.replace("[", "").replace("]", "").split(":")]
    return max(ul) - min(ul) + 1


def _parse_port_shape(dtypep, typetable):
    """"""
    table_type = typetable[dtypep]

    if table_type["name"] == "":
        decl_range = _range_to_int(table_type.get("declRange", "[0:0]"))

        ref_type = table_type["refDTypep"]
        ref_declrange = _range_to_int(typetable[ref_type].get("declRange", "[0:0]"))
        if ref_declrange == 1:
            return (decl_range,)
        else:
            return (decl_range, _parse_port_shape(ref_type, typetable))

    return ()


def _parse_port_width(dtypep, typetable):
    """Recursively find a ports type. Recursion needed for arrays."""
    table_type = typetable[dtypep]
    if table_type["name"] == "":
        return _parse_port_width(table_type["refDTypep"], typetable)
    return _range_to_int(table_type.get("range", "[0:0]"))


def _flatten(data):
    """Flatten a tuple."""
    result = []
    for item in data:
        if isinstance(item, tuple):
            result.extend(_flatten(item))
        else:
            result.append(item)
    return tuple(result)


@dataclass
class Port:
    """HDL module port configuration."""

    # name: str
    width: int
    direction: Literal["input", "output"]  # input or output
    shape: tuple = ()  # Support scalars or arrays


def _parse_port_entry(entry: dict[str], typetable: dict[str]):
    """"""
    name = entry["name"]
    direction = str(entry["direction"]).lower()
    # dtypep = typetable[entry["dtypep"]]
    dtypep = entry["dtypep"]

    width = _parse_port_width(dtypep, typetable)
    shape = _flatten(_parse_port_shape(dtypep, typetable))
    return Port(width=width, direction=direction, shape=shape)


def read_ports(json_tree: dict) -> dict[str, Port]:
    """"""
    modulesp = json_tree["modulesp"][0]
    typetable = {t["addr"]: t for t in json_tree["miscsp"][0]["typesp"]}
    stmtsp: list[dict[str]] = modulesp["stmtsp"]

    # Read in parameters and parse the value.
    ports = {
        entry["name"]: _parse_port_entry(entry, typetable)
        for entry in stmtsp
        if entry.get("varType", "Not") == "PORT"
    }

    return ports


@dataclass
class Model:
    """HDL module configuration."""

    source: Path = None
    parameters: dict[str, Parameter] = field(default_factory=dict)
    include_dirs: list[Path] = field(default_factory=list)
    trace: Literal[None, "vcd", "fst", "notrace"] = None
    verilator_args: list = field(default_factory=list)
    ports: dict[str, Port] = field(default_factory=dict)


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


def fmt_param_val(param: Parameter):
    """"""
    if isinstance(param, str):
        return f'"{param}"'

    return str(param)


def fmt_param_varg(param: Parameter):
    """"""
    if isinstance(param, list):
        header = "'{"
        tail = "}"
        inits = ", ".join([fmt_param_varg(p) for p in param])
        return f"{header}{inits}{tail}"

    return fmt_param_val(param)


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
    models: dict[str, Model] = field(default_factory=dict)

    @classmethod
    def from_pyproject(cls, pyproject_path: Path = Path("pyproject.toml")):
        """Read in the complete configuration for all models."""
        pyproject_path = pyproject_path.absolute()

        with open(pyproject_path, "rb") as fp:
            pyproject = tomllib.load(fp)
        dspsim_tool_config: dict = pyproject["tool"]["dspsim"]
        tool_config = cls.from_dict(dspsim_tool_config)

        # fix relative include dirs.
        tool_config.include_dirs = [
            _get_abs_path(idir, pyproject_path) for idir in tool_config.include_dirs
        ]
        # Collect all sources from sources field. globbed.
        # Relative to pyproject.toml path if not absolute.
        found_sources: set[Path] = set()
        for source in tool_config.sources:
            # Glob every source, and add all options to the set.
            for filename in glob.glob(str(source)):
                # Add to the set.
                found_sources.add(_get_abs_path(Path(filename), pyproject_path))

        # Add any extra sources specified in the models config.
        for model_name, model in tool_config.models.items():
            # print(model)
            if model.source:
                source_name = model.source
            else:
                source_name = Path(model_name)
            found = _find_source(
                source_name,
                found_sources,
                tool_config.include_dirs,
                pyproject_path,
            )
            model.source = found
            found_sources.add(found)

        tool_config.sources = found_sources
        # Get all of the default parameters for each source.
        # Set up default models.
        default_source_params: dict[Path, dict[str, Parameter]] = {}
        default_models: dict[str, Model] = {}
        for source in tool_config.sources:
            _json_tree = generate_tree(source)
            default_params = read_parameters(_json_tree)
            # Add the default models.
            default_models[source.stem] = Model(
                source,
                parameters={
                    k: v
                    for k, v in tool_config.parameters.items()
                    if k in default_params
                },
                include_dirs=tool_config.include_dirs.copy(),
                trace=tool_config.trace,
                verilator_args=tool_config.verilator_args.copy(),
            )
            default_source_params[source] = default_params.copy()

        final_models: dict[str, Model] = default_models.copy()
        for name, model in tool_config.models.items():
            params = default_source_params[model.source]
            # Add new model
            final_models[name] = Model(
                model.source,
                params.copy(),
                include_dirs=model.include_dirs.copy(),
                trace=model.trace,
                verilator_args=model.verilator_args.copy(),
            )
            for param_name in params:
                if param_name in tool_config.parameters:
                    final_models[name].parameters[param_name] = tool_config.parameters[
                        param_name
                    ]
                if param_name in model.parameters:
                    final_models[name].parameters[param_name] = model.parameters[
                        param_name
                    ]
            # final_models[name].parameters =
            final_models[name].include_dirs.extend(tool_config.include_dirs)
            if name not in default_models:
                for k, v in model.parameters.items():
                    if k in params:
                        params[k] = v

                final_models[name].trace = (
                    model.trace if model.trace else tool_config.trace
                )

            else:
                # override parameters
                for param_name, value in model.parameters.items():
                    if param_name in params:
                        final_models[name].parameters[param_name] = value
                # Override trace
                if model.trace:
                    final_models[name].trace = model.trace
                # Extend verilator args.
                final_models[name].verilator_args.extend(model.verilator_args)

        # And finally, run verilator with all the overrides.
        for model_name, model in final_models.items():
            # We can't parse arrays on the command line.
            param_args = [
                f"-G{pn}={fmt_param_varg(pv)}"
                for pn, pv in model.parameters.items()
                if not isinstance(pv, list)
            ]
            model.verilator_args.extend(param_args)

            json_tree = generate_tree(model.source, verilator_args=model.verilator_args)
            model.parameters = read_parameters(json_tree)
            model.ports = read_ports(json_tree)
        tool_config.models = final_models

        return tool_config


import jinja2

_template_env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(Path(__file__).parent / "templates")
)


def render_template(template: str, name: str, config: Model) -> str:
    """"""
    return _template_env.get_template(template).render(name=name, config=config)


def run_generate_model(pyproject_path: Path, json_tool_cfg: Path, output_dir: Path):
    """"""
    config = Config.from_pyproject(pyproject_path)
    with open(json_tool_cfg, "w") as fp:
        fp.write(config.to_json(indent=4))

    for name, model in config.models.items():
        prefix = f"V{name}"
        gen_file = output_dir / f"{prefix}.dir/{name}.h"
        os.makedirs(gen_file.parent.absolute(), exist_ok=True)
        with open(gen_file, "w") as fp:
            fp.write(render_template("model.cpp.jinja", name=name, config=model))


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


if __name__ == "__main__":
    exit(main())
