import logging

logger = logging.getLogger(__name__)

import subprocess
from pathlib import Path
from functools import cache, cached_property
import os
import sys
import json
import shutil
from tempfile import TemporaryDirectory

from dataclasses import dataclass, field
from dspsim.config import Parameter, Port, ModuleConfig
from typing import Literal
import numpy as np


@cache
def verilator_bin() -> Path:
    """"""
    if "VERILATOR_ROOT" in os.environ:
        return Path(os.environ["VERILATOR_ROOT"]) / "bin/verilator"
    else:
        # Assume it's on the path? Search for it?
        return "verilator"


def verilator(args: list[str]):
    """"""
    cmd = [verilator_bin()] + args
    subprocess.run(cmd)


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


def _range_to_int(r: str) -> int:
    """"""
    ul = [int(x) for x in r.replace("[", "").replace("]", "").split(":")]
    return max(ul) - min(ul) + 1


class VModelTree:
    tree_file: Path
    tree_meta_file: Path

    _name: str

    # Raw json content.
    _content: dict[str]
    _meta_content: dict[str]

    # Raw submodules.
    _miscsp: dict[str]  # Miscs?
    _modulesp: dict[str]  # Modules
    _stmtsp: dict[str]  # Statements?
    _typetable: dict[str, dict[str]]  # Typetable

    def __init__(
        self, tree_file: Path, tree_meta_file: Path = None, source: Path = None
    ):
        """Read in json content."""
        if tree_meta_file is None:
            tree_meta_file = tree_file.with_suffix(".meta.json")
        self.tree_file = tree_file
        self.tree_meta_file = tree_meta_file

        with open(self.tree_file) as fp:
            self._content = json.load(fp)
            # raise Exception(self._content)
        # with open(self.tree_meta_file) as fp:
        #     self._meta_content = json.load(fp)

        self._miscsp = self._content["miscsp"][0]
        self._modulesp = self._content["modulesp"][0]
        self._stmtsp: list[dict[str]] = self._modulesp["stmtsp"]
        self._typetable = {t["addr"]: t for t in self._miscsp["typesp"]}

    @classmethod
    def generate(
        cls,
        source: Path,
        parameters: dict[str, Parameter] = {},
        include_dirs: list[Path] = [],
        verilator_args: list[str] = [],
        output_file: Path = None,
    ):
        """"""

        with TemporaryDirectory() as _tmpdir:
            tmpdir = Path(_tmpdir)
            if len(parameters):
                # We have to check len of parameters before calling, otherwise we recurse endlessly.
                default_tree = VModelTree.generate(
                    source, include_dirs=include_dirs, verilator_args=verilator_args
                )
                # Only allow overriding valid parameters. Throw error if invalid?
                param_args = [
                    f"-G{k}={v.vvalue}"
                    for k, v in parameters.items()
                    if k in default_tree.parameters
                ]
                verilator_args.extend(param_args)

            verilator_args.extend(["--quiet", "--json-only", "--Mdir", tmpdir])
            inc_dirs = [f"-I{d.absolute()}" for d in include_dirs]
            verilator_args.extend(inc_dirs)
            verilator([source] + verilator_args)

            tree_file = tmpdir / f"V{source.stem}.tree.json"
            tree_meta_file = tree_file.with_suffix(".meta.json")

            # Copy file if requested.
            if output_file:
                shutil.copy(tree_file, output_file)

            obj = cls(tree_file, tree_meta_file)
            return obj

    def to_config(self, cls: type[ModuleConfig]) -> ModuleConfig:
        """"""
        return cls(self.name, self.source, self.parameters.copy(), self.ports.copy())

    # @cached_property
    # def source(self) -> Path:
    #     """"""
    #     for f in self._meta_content["files"].values():
    #         filename = Path(f["filename"])
    #         if filename.exists() and filename.suffix in [".v", ".sv"]:
    #             return Path(f["realpath"])

    @cached_property
    def name(self) -> str:
        """"""
        return self._modulesp["name"]

    @cached_property
    def parameters(self) -> dict[str, Parameter]:
        """"""
        _type_names = {
            "logic": _int_from_str,
            "int": _int_from_str,
            "real": float,
            "string": lambda x: str(x)
            .replace("\\", "")
            .replace('"', "")
            .replace("'", ""),
        }

        def _parse_value(valuep):
            """Parse parameter value using typetable."""
            return _type_names[self._typetable[valuep["dtypep"]]["name"]](
                valuep["name"]
            )

        def _parse_param_value(valuep):
            """Recursively read in parameter definition to build multi-dimensional arrays."""
            if "initsp" in valuep:
                return [_parse_param_value(i["valuep"][0]) for i in valuep["initsp"]]

            return _parse_value(valuep)

        # Read in parameters and parse the value.
        return {
            entry["name"]: Parameter(
                entry["name"], np.array(_parse_param_value(entry["valuep"][0]))
            )
            for entry in self._stmtsp
            if entry.get("isGParam", False)
        }

    @cached_property
    def ports(self) -> dict[str, Port]:
        """"""

        def _parse_port_shape(dtypep):
            """"""
            table_type = self._typetable[dtypep]

            if table_type["name"] == "":
                decl_range = _range_to_int(table_type.get("declRange", "[0:0]"))

                ref_type = table_type["refDTypep"]
                ref_declrange = _range_to_int(
                    self._typetable[ref_type].get("declRange", "[0:0]")
                )
                if ref_declrange == 1:
                    return (decl_range,)
                else:
                    return (decl_range, _parse_port_shape(ref_type))

            return ()

        def _parse_port_width(dtypep):
            """Recursively find a ports type. Recursion needed for arrays."""
            table_type = self._typetable[dtypep]
            if table_type["name"] == "":
                return _parse_port_width(table_type["refDTypep"])
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

        def _parse_port_entry(entry: dict[str]):
            """"""
            name = entry["name"]
            direction = str(entry["direction"]).lower()
            # dtypep = typetable[entry["dtypep"]]
            dtypep = entry["dtypep"]

            width = _parse_port_width(dtypep)
            shape = _flatten(_parse_port_shape(dtypep))
            return Port(name=name, width=width, direction=direction, shape=shape)

        return {
            entry["name"]: _parse_port_entry(entry)
            for entry in self._stmtsp
            if entry.get("varType", "Not") == "PORT"
        }


# def read_tree(tree_file: Path) -> dict:
#     """Read"""
#     with open(tree_file, "r") as fp:
#         return json.load(fp)


# def write_tree(tree_file: Path, content: dict):
#     """"""
#     with open(tree_file, "w") as fp:
#         json.dump(content, fp)


# def generate_tree(
#     source: Path, output_file: Path = None, *, verilator_args: list[str] = []
# ) -> dict:
#     """"""
#     args = [source, "--json-only"]
#     args.extend(verilator_args)
#     with TemporaryDirectory() as _tmpdir:
#         tmpdir = Path(_tmpdir)

#         args.extend(["--Mdir", tmpdir])
#         verilator(args)

#         tmp_file = tmpdir / f"V{source.stem}.tree.json"

#         content = read_tree(tmp_file)
#         # Copy file if requested.
#         if output_file:
#             shutil.copy(tmp_file, output_file)

#     return content
