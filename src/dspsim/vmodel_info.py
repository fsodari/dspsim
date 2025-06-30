"""
Parse verilog hdl models using verilator to get information about parameters and ports.
"""

from dataclasses import dataclass, field
from pathlib import Path
import numpy as np
import numpy.typing as npt
from typing import Any
from tempfile import TemporaryDirectory
import json
from .verilator import verilate

from dataclass_wizard import JSONSerializable
# from .config import Parameter, Port

# ParameterValue: TypeAlias = npt.NDArray


def _uint_width(width: int) -> int:
    if width <= 8:
        return 8
    elif width <= 16:
        return 16
    elif width <= 32:
        return 32
    elif width <= 64:
        return 64
    else:
        raise Exception("Int too big")


_uint_str = {8: "uint8_t", 16: "uint16_t", 32: "uint32_t", 64: "uint64_t"}


def _vvalue_str(value: npt.NDArray):
    if value.shape:
        header = "'{"
        tail = "}"
        inits = ", ".join([_vvalue_str(p) for p in value])
        return f"{header}{inits}{tail}"

    if value.dtype.kind == "U":
        return f'"{value}"'

    return str(value)


def _ctype_str(value: npt.NDArray):
    """"""
    if value.shape:
        header = "std::array<"
        tail = f", {len(value)}>"
        # inits = "".join([_cpp_val_type(p) for p in value])
        return f"{header}{_ctype_str(value[0])}{tail}"
    if value.dtype.kind == "U":
        return "std::string"
    elif value.dtype.kind == "i":
        return "int"
    elif value.dtype.kind == "f":
        return "float"
    else:
        raise Exception("Invalid type.")


def _cvalue_str(value: npt.NDArray):
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
    value: npt.NDArray

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
    direction: str  # input or output
    shape: tuple = ()  # Support scalars or arrays

    @property
    def ctype(self) -> str:
        """"""
        return _uint_str[_uint_width(self.width)]

    # @property
    # def cvalue(self) -> str:
    #     """"""

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


@dataclass
class VTree:
    modulesp: dict[str, list[dict[str, str]]]


TypesT = list[dict[str, str]]
MiscsT = list[dict[str, TypesT]]
StmtsT = list[dict[str, str]]
TypeTableT = dict[str, dict[str, str]]


def _parse_type_table(
    typesp: TypesT,
) -> dict[str, dict[str, str]]:
    """Format the json type table as a dict with addr as keys."""
    return {x["addr"]: x for x in typesp}


def _parse_parameters(stmtsp: StmtsT, type_table: TypeTableT) -> dict[str, Parameter]:
    """Parse the parameters with values from the json content."""

    def _parse_value(valuep: dict[str, str]) -> int | float | str:
        """Parse parameter value."""
        type_name = type_table[valuep["dtypep"]]["name"]
        value_str = valuep["name"]

        def _parse_int(s: str) -> int:
            def sign_extend(value, bits):
                sign_bit = 1 << (bits - 1)
                return (value & (sign_bit - 1)) - (value & sign_bit)

            fmt, val = s.split("h", maxsplit=1)
            nbits = int(fmt.split("'")[0])
            result = int(val, base=16)
            return sign_extend(result, nbits)

        def _parse_str(s: str) -> str:
            """Clean up string"""
            return str(s).replace("\\", "").replace('"', "").replace("'", "")

        options = {
            "logic": _parse_int,
            "int": _parse_int,
            "integer": _parse_int,
            "real": float,
            "string": _parse_str,
        }

        return options[type_name](value_str)

    def _parse_param_value(valuep):
        """Recursively read in parameter definition to build multi-dimensional arrays."""
        if "initsp" in valuep:
            return [_parse_param_value(i["valuep"][0]) for i in valuep["initsp"]]

        return _parse_value(valuep)

    return {
        x["name"]: Parameter(x["name"], np.array(_parse_param_value(x["valuep"][0])))
        for x in stmtsp
        if x.get("isGParam", False)
    }


def _parse_ports(stmtsp: StmtsT, type_table: TypeTableT) -> dict[str, Port]:
    """Parse port information from the json content."""

    def _range_to_int(r: str) -> int:
        """"""
        ul = [int(x) for x in r.replace("[", "").replace("]", "").split(":")]
        return max(ul) - min(ul) + 1

    def _parse_port_shape(dtypep):
        """Recursively search the content to determine a port's shape."""
        table_type = type_table[dtypep]

        if table_type["name"] == "":
            decl_range = _range_to_int(table_type.get("declRange", "[0:0]"))

            ref_type = table_type["refDTypep"]
            ref_declrange = _range_to_int(
                type_table[ref_type].get("declRange", "[0:0]")
            )
            if ref_declrange == 1:
                return (decl_range,)
            else:
                return (decl_range, _parse_port_shape(ref_type))

        return ()

    def _parse_port_width(dtypep):
        """Recursively find a ports type. Recursion needed for arrays."""
        table_type = type_table[dtypep]
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

    def _parse_port_entry(entry: dict[str, str]):
        """"""
        name = entry["name"]
        direction = str(entry["direction"]).lower()
        dtypep = entry["dtypep"]

        width = _parse_port_width(dtypep)
        shape = _flatten(_parse_port_shape(dtypep))
        return Port(name=name, width=width, direction=direction, shape=shape)

    return {
        entry["name"]: _parse_port_entry(entry)
        for entry in stmtsp
        if entry.get("varType", "") == "PORT"
    }


@dataclass
class ModuleConfig(JSONSerializable):
    """Verilog/HDL model interface information."""

    name: str
    source: Path
    include_dirs: list[Path] = field(default_factory=list)
    parameters: dict[str, Parameter] = field(default_factory=dict)
    ports: dict[str, Port] = field(default_factory=dict)
    trace: str | None = None  # vcd, fst, notrace, None
    verilator_args: list[str] = field(default_factory=list)

    # Raw json content.
    _content: dict[str, dict[str, str]] | None = field(
        repr=False, init=False, default_factory=lambda: None
    )

    @classmethod
    def load_model(
        cls,
        source: Path,
        include_dirs: list[Path] = [],
        parameters: dict[str, Any] = {},
        trace: str | None = None,
        verilator_args: list[str] = [],
        filter_parameters: bool = True,
    ):
        """Load model from verilog source. Optionally filter out invalid parameters, useful when using globally defined parameters."""

        # Filter out invalid parameters. Load the parameter names from the default model.
        if filter_parameters and len(parameters):
            default_model = ModuleConfig.load_model(
                source=source,
                include_dirs=include_dirs,
                parameters={},
                trace=trace,
                verilator_args=verilator_args,
            )
            valid_parameters = {
                k: v for k, v in parameters.items() if k in default_model.parameters
            }
        else:
            valid_parameters = parameters
            #

        # Save generated output files in a temporary directory.
        with TemporaryDirectory() as tmpdir:
            # Verilate the model using --json-only
            verilate(
                sources=[source],
                output_dir=Path(tmpdir),
                include_dirs=include_dirs,
                trace=trace,
                parameters=valid_parameters,
                verilator_args=verilator_args + ["--json-only", "--quiet"],
            )

            # Find the generated json file.
            json_file = list(Path(tmpdir).glob("*.tree.json"))[0]
            # json_meta_file = Path(tmpdir).glob("*.tree.meta.json")
            with open(json_file) as fp:
                _content = json.load(fp)

        # Parse contents
        _name = _content["modulesp"][0]["name"]
        _type_table = _parse_type_table(_content["miscsp"][0]["typesp"])
        _stmtsp = _content["modulesp"][0]["stmtsp"]
        _parameters = _parse_parameters(_stmtsp, _type_table)
        _ports = _parse_ports(_stmtsp, _type_table)

        inst = cls(
            name=_name,
            source=source,
            include_dirs=include_dirs,
            trace=trace,
            parameters=_parameters,
            ports=_ports,
            verilator_args=verilator_args,
        )
        inst._content = _content
        return inst

    def port_info(self) -> str:
        dictinfo = {k: vars(v) for k, v in self.ports.items()}
        return str(dictinfo)
