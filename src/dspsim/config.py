"""DSPSim from pyproject.toml"""

from pathlib import Path
from dataclasses import dataclass, field
import tomllib
import glob
from typing import TypeAlias, Literal
import json
from tempfile import TemporaryDirectory
import shutil
from functools import cache
import numpy as np
from typing import Any
import numpy.typing as npt

# from dspsim import verilator

# from .util import cmake_dir
ParameterValue: TypeAlias = npt.NDArray


@cache
def _uint_width(width: int) -> int:
    if width <= 8:
        return 8
    elif width <= 16:
        return 16
    elif width <= 32:
        return 32
    elif width <= 64:
        return 64


_uint_str = {8: "uint8_t", 16: "uint16_t", 32: "uint32_t", 64: "uint64_t"}


def _vvalue_str(value: ParameterValue):
    if value.shape:
        header = "'{"
        tail = "}"
        inits = ", ".join([_vvalue_str(p) for p in value])
        return f"{header}{inits}{tail}"

    if value.dtype.kind == "U":
        return f'"{value}"'

    return str(value)


def _ctype_str(value: ParameterValue):
    """"""
    if value.shape:
        header = "std::array<"
        tail = f", {len(value)}>"
        # inits = "".join([_cpp_val_type(p) for p in value])
        return f"{header}{_ctype_str(value[0])}{tail}"
    if value.dtype.kind == "U":
        return f"std::string"
    elif value.dtype.kind == "i":
        return "int"
    elif value.dtype.kind == "f":
        return "float"


def _cvalue_str(value: ParameterValue):
    """"""
    if value.shape:
        header = "{" + "{"
        tail = "}" + "}"
        inits = ", ".join([_cvalue_str(p) for p in value])
        return f"{header}{inits}{tail}"
    if value.dtype.kind == "U":
        return f'"{value}"'
    else:
        return str(value)


@dataclass
class Parameter:
    name: str
    value: ParameterValue

    @property
    def ctype(self) -> str:
        """C++ type as a string."""
        return _ctype_str(self.value)

    @property
    def cvalue(self) -> str:
        """C++ initializer for value. As a string."""
        return _cvalue_str(self.value)

    @property
    def vtype(self) -> str:
        """Verilog type as a string."""
        _types = {"i": "int", "f": "float", "U": "string"}
        return _types[self.value.dtype.kind]

    @property
    def vvalue(self) -> str:
        """"""
        return _vvalue_str(self.value)

    @property
    def sv_mod_def(self) -> str:
        """"""
        return f"parameter {self.vtype} {self.name} = {self.vvalue}"

    @property
    def vm_def(self) -> str:
        """"""
        return f"static const {self.ctype} {self.name} = {self.cvalue}"

    def info(self) -> str:
        """Return main information about the parameter."""
        return f"Parameter(name={self.name}, value={self.value}, dtype={self.value.dtype}, shape={self.value.shape})"


def _vm_ctor_arg_str(width: int, shape: tuple) -> str:
    """"""
    if shape:
        return f"std::array<{_vm_ctor_arg_str(width, shape[1:])}, {shape[0]}>"
    return f"dspsim::Signal<{_uint_str[_uint_width(width)]}>*"


@dataclass
class Port:
    """HDL module port configuration."""

    name: str
    width: int
    direction: Literal["input", "output"]  # input or output
    shape: tuple = ()  # Support scalars or arrays

    @property
    def ctype(self) -> str:
        """"""
        return _uint_str[_uint_width(self.width)]

    @property
    def cvalue(self) -> str:
        """"""

    @property
    def vm_port_decl(self) -> str:
        """"""
        port_dir = "Input" if self.direction.startswith("i") else "Output"
        decl = f"dspsim::{port_dir}<{self.ctype}"
        for s in self.shape:
            decl += f"[{s}]"
        return f"{decl}> _{self.name}"

    @property
    def vm_ctor_arg(self) -> str:
        """"""
        if self.shape:
            return f"{_vm_ctor_arg_str(self.width, self.shape)} &{self.name}"
        else:
            return f"dspsim::Signal<{self.ctype}> &{self.name}"

    @property
    def sv_range(self) -> str:
        if self.width <= 1:
            return ""
        return f"[{self.width-1}:0]"

    @property
    def sv_mod_def(self) -> str:
        direction_fmt = "input " if self.direction.startswith("i") else "output"
        width_fmt = f" {self.sv_range}" if self.width > 1 else ""
        unpacked_fmt = "".join([f"[{s}]" for s in self.shape])

        return f"{direction_fmt} logic{width_fmt} {self.name}{unpacked_fmt}"


from dataclass_wizard import JSONSerializable


@dataclass
class ModuleConfig(JSONSerializable):
    """
    The basic information about an hdl source module.
    This can be read from a verilog file, a verilated module,
    Or it can be used from python to create a verilated module or
    a framework model.

    This interface should be compatible with verilog, system verilog, vhdl, etc.
    And other simulators besides verilator could be supported.
    """

    # Parsed and formatted information
    name: str  # This should always be the same as source.stem ?
    source: Path
    parameters: dict[str, Parameter]  # Parsed from type table
    ports: dict[str, Port]  #
    trace: Literal[None, "vcd", "fst", "notrace"] = None
    include_dirs: list[Path] = field(default_factory=list)
    verilator_args: list[str] = field(default_factory=list)

    @classmethod
    def from_verilator(
        cls,
        source: Path,
        parameters: dict[str, Parameter] = {},
        trace: Literal[None, "vcd", "fst", "notrace"] = None,
        include_dirs: list[Path] = [],
        verilator_args: list[str] = [],
    ):
        """"""
        from dspsim.verilator import VModelTree

        tree = VModelTree.generate(
            source,
            parameters=parameters,
            include_dirs=include_dirs,
            verilator_args=verilator_args.copy(),
        )
        # raise Exception(source)
        obj = cls(
            name=tree.name,
            source=source,
            parameters=tree.parameters,
            ports=tree.ports,
            trace=trace,
            include_dirs=include_dirs,
            verilator_args=verilator_args,
        )
        return obj

    def port_info(self) -> str:
        dictinfo = {k: vars(v) for k, v in self.ports.items()}
        return str(dictinfo)
        # return self.to_json()
