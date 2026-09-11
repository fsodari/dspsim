"""Verilator interface."""

import json
import os
import subprocess
from collections.abc import Mapping
from contextlib import nullcontext
from pathlib import Path
from tempfile import TemporaryDirectory


def verilator_root() -> Path:
    return Path(os.getenv("VERILATOR_ROOT", ""))


def verilator_bin() -> Path:
    return verilator_root() / "bin" / "verilator"


def verilator(args: list[str], capture_output: bool = False, check: bool = False):
    """
    Run verilator with the given args.

    Returns the result of subprocess.run.
    """

    # # Set VERILATOR_ROOT in the environment. Verilator usually requires this.
    # os.environ["VERILATOR_ROOT"] = str(verilator_root())

    # Prepend the verilator_exe path to the verilator args.
    command_args = [verilator_bin()] + args

    # Run using subprocess.
    return subprocess.run(command_args, capture_output=capture_output, check=check)


class VerilatorError(Exception):
    """Exception raised when Verilator fails."""


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
    if include_dirs is None:
        include_dirs = []
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
    if parameters is None:
        parameters = {}

    args.extend([f"-G{name}={value}" for name, value in parameters.items()])

    # Extra verilator args.
    if verilator_args is None:
        verilator_args = []
    args.extend(verilator_args)

    result = verilator(args, capture_output=True, check=False)

    if result.returncode:
        raise VerilatorError(result.stderr.decode())

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

    verilator_args.append("--json-only")
    verilator_args.append("--quiet")

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
            return json.load(f)
