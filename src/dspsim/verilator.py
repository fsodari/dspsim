"""Verilator interface."""

import json
import os
import subprocess
from collections.abc import Mapping
from contextlib import nullcontext
from dataclasses import dataclass
from pathlib import Path
from tempfile import TemporaryDirectory

from dspsim.module_info import ModuleInfo, Parameter, ParamValueT, Port

try:
    # Check if verilator package was installed. VERILATOR_ROOT is set when this is imported.
    import verilator as _verilator

    assert _verilator.verilator_root() == Path(os.environ["VERILATOR_ROOT"])
except ImportError:
    # Use verilator installed on system. VERILATOR_ROOT must be set.
    try:
        _verilator_root = os.environ.get("VERILATOR_ROOT")
    except KeyError:
        raise ModuleNotFoundError("Verilator not found and VERILATOR_ROOT is not set.")


def verilator_root() -> Path:
    return Path(os.environ["VERILATOR_ROOT"])


def verilator_bin() -> Path:
    return verilator_root() / "bin" / "verilator"


def verilator(args: list[str], capture_output: bool = False, check: bool = False):
    """
    Run verilator with the given args.

    Returns the result of subprocess.run.
    """

    # Prepend the verilator_exe path to the verilator args.
    command_args = [verilator_bin()] + args

    # Run using subprocess.
    return subprocess.run(command_args, capture_output=capture_output, check=check)


class VerilatorError(Exception):
    pass


def verilate(
    sources: list[Path],
    output_dir: Path | None = None,
    include_dirs: list[Path] | None = None,
    parameters: Mapping[str, str | int | float] | None = None,
    prefix: str | None = None,
    top_module: str | None = None,
    trace_vcd: bool = False,
    trace_fst: bool = False,
    threads: bool = False,
    trace_threads: bool = False,
    verilator_args: list[str] | None = None,
):
    """Run verilator with common options, converting python data types into appropriate arguments."""
    args = [s.as_posix() for s in sources]

    # Output directory
    if output_dir:
        args.extend(["--Mdir", output_dir.as_posix()])

    # Include directories.
    if include_dirs:
        args.extend([f"-I{i}" for i in include_dirs])

    # Override generated module prefix
    if prefix:
        args.extend(["--prefix", prefix])

    # Specify module
    if top_module:
        args.extend(["--top-module", top_module])

    # Choose to use either vcd or fst tracing.
    if trace_vcd:
        args.append("--trace-vcd")
    elif trace_fst:
        args.append("--trace-fst")

    if threads:
        args.append("--threads")
    if trace_threads:
        args.append("--trace-threads")

    # Parameters. Scalar parameters only :(
    if parameters:
        args.extend([f"-G{name}={value}" for name, value in parameters.items()])

    # Extra verilator args.
    if verilator_args:
        args.extend(verilator_args)

    result = verilator(args, capture_output=True, check=False)

    if result.returncode:
        raise VerilatorError(f"Verilate failed: {result.stderr.decode()}")

    return result.stdout.decode()


def verilate_json(
    sources: list[Path],
    output_dir: Path | None = None,
    include_dirs: list[Path] | None = None,
    parameters: Mapping[str, str | int | float] | None = None,
    prefix: str | None = None,
    top_module: str | None = None,
    trace_vcd: bool = False,
    trace_fst: bool = False,
    threads: bool = False,
    trace_threads: bool = False,
    verilator_args: list[str] | None = None,
):
    """Run verilator --json-only and return the JSON output."""
    if verilator_args is None:
        verilator_args = []

    verilator_args.extend(
        [
            "--json-only",
            "--quiet",
        ]
    )

    odir_ctx = TemporaryDirectory() if output_dir is None else nullcontext(output_dir)

    with odir_ctx as odir:
        # Run verilator --json-only
        verilate(
            sources=sources,
            output_dir=Path(odir),
            include_dirs=include_dirs,
            parameters=parameters,
            prefix=prefix,
            top_module=top_module,
            trace_vcd=trace_vcd,
            trace_fst=trace_fst,
            threads=threads,
            trace_threads=trace_threads,
            verilator_args=verilator_args,
        )

        # Read and return the JSON output.
        with open(Path(odir) / f"V{sources[0].stem}.tree.json") as f:
            model_data = json.load(f)

        # Read the metadata file.
        with open(Path(odir) / f"V{sources[0].stem}.tree.meta.json") as f:
            metadata = json.load(f)

        return model_data, metadata


_cnv_type_table: dict[str, type[int | str | float]] = {
    "bit": int,
    "logic": int,
    "int": int,
    "real": float,
    "string": str,
}


@dataclass
class DType:
    keyword: str
    width: int
    signed: bool
    shape: tuple[int, ...]

    @property
    def cnv_type(self) -> type[int | str | float]:
        return _cnv_type_table[self.keyword]


def _range_to_int(range_str: str) -> int:
    """
    Convert a range string of the form '[upper:lower]' to an integer width.
    Unpacked arrays use the larger number in the lower bound, so this takes the absolute value of the difference.
    """
    upper, lower = map(int, range_str.strip("[]").split(":"))
    return abs(upper - lower) + 1


def _parse_shape(type_entry: dict, type_table: dict):
    """Parse the shape of an unpacked array type from the given type dictionary."""
    # If there is no range, it means this type is scalar.
    if "declRange" not in type_entry:
        return ()
    # Traverse the reference dtype to determine the shape.
    return (
        _range_to_int(type_entry["declRange"]),
        *_parse_shape(type_table[type_entry["refDTypep"]], type_table),
    )


def _sign_extend(value, bits):
    sign_bit = 1 << (bits - 1)
    return (value & (sign_bit - 1)) - (value & sign_bit)


def _parse_value(value_entry: dict[str, str], type_table: dict) -> int | float | str:
    """Parse parameter value."""
    signed = type_table[value_entry["dtypep"]].get("signed", False)
    keyword = type_table[value_entry["dtypep"]]["keyword"]
    value_str = value_entry["name"]

    match keyword:
        case "bit" | "logic" | "int" | "integer":
            nbits = value_str.split("'")[0]
            result = int(value_str.split("h")[-1], base=16)
            if signed:
                result = _sign_extend(result, int(nbits))

        case "real":
            result = float(value_str)
        case "string":
            result = str(value_str).replace("\\", "").replace('"', "").replace("'", "")
        case _:
            raise ValueError(f"Unsupported keyword: {keyword}")

    return result


def _parse_param_value(value_entry, type_table) -> ParamValueT:
    """Recursively read in parameter definition to build multi-dimensional arrays."""
    return _parse_value(value_entry, type_table)
    # if "initsp" not in value_entry:
    #     return _parse_value(value_entry, type_table)

    # return [
    #     _parse_param_value(i["valuep"][0], type_table) for i in value_entry["initsp"]
    # ]


def _parse_dtype(type_entry: dict, type_table: dict):
    """Parse an unpacked array type from the given type dictionary."""

    def _find_child_type(x: dict):
        if "refDTypep" not in x:
            return type_table[x["dtypep"]]
        return _find_child_type(type_table[x["refDTypep"]])

    _child_type = _find_child_type(type_entry)
    keyword = _child_type["keyword"]
    match keyword:
        case "int" | "logic" | "bit":
            width = _range_to_int(_child_type.get("range", "[0:0]"))
        case _:
            width = -1

    return DType(
        keyword=keyword,
        width=width,
        signed=_child_type.get("signed", False),
        shape=_parse_shape(type_entry, type_table),
    )


def parse_module_json(model_data, metadata):
    """Parse complete module info from the given JSON content."""
    statements = model_data["modulesp"][0]["stmtsp"]
    types_table = {t["addr"]: t for t in model_data["miscsp"][0]["typesp"]}

    parameters = {
        s["origName"]: Parameter(
            name=s["origName"],
            keyword=_parse_dtype(types_table[s["dtypep"]], types_table).keyword,
            signed=_parse_dtype(types_table[s["dtypep"]], types_table).signed,
            width=_parse_dtype(types_table[s["dtypep"]], types_table).width,
            value=_parse_param_value(s["valuep"][0], types_table),
        )
        for s in statements
        if s.get("isGParam", False)
    }

    ports = {
        s["origName"]: Port(
            name=s["origName"],
            keyword=_parse_dtype(types_table[s["dtypep"]], types_table).keyword,
            signed=_parse_dtype(types_table[s["dtypep"]], types_table).signed,
            width=_parse_dtype(types_table[s["dtypep"]], types_table).width,
            direction=str(s["direction"]).lower(),
            shape=_parse_dtype(types_table[s["dtypep"]], types_table).shape,
        )
        for s in statements
        if s.get("varType", "") == "PORT"
    }

    return ModuleInfo(
        name=model_data["modulesp"][0]["name"],
        source=Path(metadata["files"]["e"]["realpath"]),
        parameters=parameters,
        ports=ports,
    )
