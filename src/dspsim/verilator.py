"""
Functions and scripts to run verilator.
"""

from pathlib import Path
import os
import sys
import subprocess


def verilator_root() -> Path:
    """VERILATOR_ROOT environment variable as a Path"""
    return Path(str(os.getenv("VERILATOR_ROOT")))


def verilator_bin() -> Path:
    """Path to verilator executable."""
    if sys.platform == "win32":
        return verilator_root() / "bin" / "verilator_bin.exe"
    else:
        return verilator_root() / "bin" / "verilator"


def verilator_include() -> Path:
    """Path to Verilator include directory."""
    return verilator_root() / "include"


def verilator(args: list[str], check: bool = False):
    """Run verilator with the given args."""
    subprocess.run([verilator_bin()] + args, check=check)


def verilate(
    sources: list[Path],
    output_dir: Path,
    include_dirs: list[Path] = [],
    prefix: str | None = None,
    top_module: str | None = None,
    trace: str | None = None,
    threads: bool = False,
    trace_threads: bool = False,
    parameters: dict[str, str] = {},
    verilator_args: list[str] = [],
) -> str:
    """Function interface to verilator with constructs for common arguments. Returns stdout."""

    args: list[str | Path] = [verilator_bin()]

    args.extend(sources)
    args.extend(["--Mdir", output_dir])
    args.extend([f"-I{i}" for i in include_dirs])

    if prefix:
        args.extend(["--prefix", prefix])
    if top_module:
        args.extend(["--top-module", top_module])

    if trace:
        if trace == "vcd":
            args.append("--trace-vcd")
        elif trace == "fst":
            args.append("--trace-fst")
        else:
            raise Exception(f"Invalid trace type: {trace}")

    if threads:
        args.append("--threads")
    if trace_threads:
        args.append("--trace-threads")

    # Parameters
    args.extend([f"-G{name}={value}" for name, value in parameters.items()])

    args.extend(verilator_args)

    print(args)
    proc = subprocess.run(args, capture_output=True, check=False)

    if proc.returncode:
        raise Exception(f"Verilate failed: {proc.stderr.decode()}")

    return proc.stdout.decode()


def verilator_cli():
    """Command line script to run verilator"""
    verilator(sys.argv[1:], check=False)
