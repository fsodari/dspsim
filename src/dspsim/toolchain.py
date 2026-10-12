"""What the builds of model extensions share: the CMake executable, the toolchain, and what CMake knows."""

import functools
import hashlib
import importlib.metadata
import json
import os
import shutil
import subprocess
import sys
import sysconfig
from collections.abc import Iterable, Sequence
from pathlib import Path
from typing import Any

import dspsim
from dspsim.templates import render_template


@functools.cache
def cmake_executable() -> str:
    """
    The CMake executable. The `cmake` package, which dspsim depends on, puts a Python script of that name on
    the path that starts the real executable: running the executable itself saves the start of an interpreter
    (60 ms), which is most of what it takes to find that a build is up to date.
    """
    try:
        import cmake
    except ImportError:
        return "cmake"
    suffix = ".exe" if sys.platform == "win32" else ""
    return str(Path(cmake.CMAKE_BIN_DIR) / f"cmake{suffix}")


@functools.cache
def toolchain() -> str:
    """
    What a built module depends on besides its own build key: dspsim, the compiler and the packages that the
    build uses. It does not change while a process runs.
    """
    installed = dspsim.include_dir().parent
    files = [*installed.glob("_framework*"), *installed.glob("*dspsim-core*")]
    if compiler := shutil.which(os.environ.get("CXX") or "c++"):
        files.append(Path(compiler).resolve())
    parts = [f"{file}:{file.stat().st_mtime_ns}" for file in sorted(files)]
    for package in ("dspsim", "nanobind", "cmake", "verilator-dspsim"):
        try:
            parts.append(f"{package}=={importlib.metadata.version(package)}")
        except importlib.metadata.PackageNotFoundError:
            pass
    return "\n".join(parts)


def dependency_compile_args(
    packages: Sequence[str],
    libraries: Sequence[str],
    output_root: Path,
    verbose: bool = False,
) -> list[str]:
    """
    The compiler arguments that the CMake packages and libraries of a module add to a compilation: their
    include directories and compile definitions. The parser needs them to read the module's header.

    Only CMake knows them. A project that finds the packages and links the libraries to a target is
    configured, once per set of dependencies, in a directory under `output_root`, and writes the include
    directories and definitions of that target.
    """
    if not packages and not libraries:
        return []
    description = [list(packages), list(libraries), os.environ.get("CMAKE_PREFIX_PATH")]
    key = hashlib.sha256((json.dumps(description) + toolchain()).encode()).hexdigest()
    output_dir = Path(output_root) / f"dependencies_{key[:12]}"
    args_file = output_dir / "compile_args.json"
    if args_file.exists():
        return json.loads(args_file.read_text())

    build_dir = output_dir / "build"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "probe.cpp").write_text("")
    (output_dir / "CMakeLists.txt").write_text(
        render_template(
            "cpp_dependencies_cmake.cmake.jinja", packages=packages, libraries=libraries
        )
    )
    out = subprocess.run(
        [
            cmake_executable(),
            "-S",
            output_dir,
            "-B",
            build_dir,
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_PREFIX_PATH={sysconfig.get_paths()['purelib']}",
        ],
        check=False,
        capture_output=True,
    )
    if verbose:
        print(out.stdout.decode())
    if out.returncode != 0:
        raise RuntimeError(
            "CMake could not configure the packages and libraries of the module "
            f"({', '.join([*packages, *libraries])}):\n{out.stderr.decode()}"
        )

    def lines(file: str) -> list[str]:
        return [line for line in (build_dir / file).read_text().splitlines() if line]

    args = [f"-I{directory}" for directory in lines("include_directories.txt")]
    args += [f"-D{definition}" for definition in lines("compile_definitions.txt")]
    args_file.write_text(json.dumps(args))
    return args


def build_key(description: Any, sources: Iterable[Path]) -> str:
    """
    A hash of everything that makes a build what it is: the description of the module (its name, its
    parameters, what is bound, the options) and the contents of its source files.

    The directory of a build and its extension module are named after it, so two builds only share them, and
    a module is only reused, if they are identical. Two classes of the same name that differ in anything
    (another source file, an edited one, other parameters) are separate builds that can both be loaded.
    What is not in the key is rebuilt in place by the build system: dspsim itself and the compiler.
    """

    def encode(value: Any) -> Any:
        # Dataclasses hold paths, and sets of headers.
        return sorted(value) if isinstance(value, (set, frozenset)) else str(value)

    hasher = hashlib.sha256()
    hasher.update(json.dumps(description, sort_keys=True, default=encode).encode())
    for source in sorted({Path(s).resolve() for s in sources}):
        hasher.update(source.as_posix().encode())
        hasher.update(source.read_bytes())
    return hasher.hexdigest()[:12]
