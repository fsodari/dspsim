import annotationlib
import hashlib
import importlib.util
import os
import re
import subprocess
import sys
import sysconfig
from pathlib import Path
from typing import TYPE_CHECKING, Any, Literal

import dotenv

import dspsim
from dspsim.framework import (
    InputArrayS8,
    InputArrayS16,
    InputArrayS32,
    InputArrayS64,
    InputArrayU8,
    InputArrayU16,
    InputArrayU32,
    InputArrayU64,
    InputS8,
    InputS16,
    InputS32,
    InputS64,
    InputU8,
    InputU16,
    InputU32,
    InputU64,
    OutputArrayS8,
    OutputArrayS16,
    OutputArrayS32,
    OutputArrayS64,
    OutputArrayU8,
    OutputArrayU16,
    OutputArrayU32,
    OutputArrayU64,
    OutputS8,
    OutputS16,
    OutputS32,
    OutputS64,
    OutputU8,
    OutputU16,
    OutputU32,
    OutputU64,
    _Module,
)
from dspsim.generate import render_template
from dspsim.module_info import ModuleInfo, Parameter, Port
from dspsim.project import load_model_info

# Projects can use a .env file to specify cache dir and other build parameters.
dotenv.load_dotenv()

# Default cache directory is .dspsim_cache in the working directory.
_DEFAULT_CACHE_DIR = Path.cwd() / ".dspsim_cache"


def cache_dir() -> Path:
    """Directory for generating project files and building models."""
    return Path(os.getenv("DSPSIM_CACHE_DIR", str(_DEFAULT_CACHE_DIR)))


# _valid_parameter_types = [int, float, str]

# Modules can have these port types. If an annotation is one of these types, it must match the one in the model.
_valid_port_types = [
    InputS8,
    InputS16,
    InputS32,
    InputS64,
    InputU8,
    InputU16,
    InputU32,
    InputU64,
    InputArrayS8,
    InputArrayS16,
    InputArrayS32,
    InputArrayS64,
    InputArrayU8,
    InputArrayU16,
    InputArrayU32,
    InputArrayU64,
    OutputS8,
    OutputS16,
    OutputS32,
    OutputS64,
    OutputU8,
    OutputU16,
    OutputU32,
    OutputU64,
    OutputArrayS8,
    OutputArrayS16,
    OutputArrayS32,
    OutputArrayS64,
    OutputArrayU8,
    OutputArrayU16,
    OutputArrayU32,
    OutputArrayU64,
]


_PORT_TYPE_NAME = re.compile(
    r"(?P<direction>Input|Output)(?P<array>Array)?(?P<sign>[US])(?P<width>\d+)"
)


def _get_class_annotations(namespace: dict[str, Any]):
    """Load the annotations of a class from cls.__dict__"""
    if annotate := annotationlib.get_annotate_from_class_namespace(namespace):
        return annotationlib.call_annotate_function(
            annotate, format=annotationlib.Format.VALUE
        )
    return {}


def _get_parameter_annotations(
    namespace: dict[str, Any], default_parameters: dict[str, Parameter]
) -> dict[str, int | float | str | None]:
    """Validate the class annotations against the default parameters and return the parameter annotations."""
    annotations = _get_class_annotations(namespace)
    params: dict[str, int | float | str | None] = {}
    for an, typ in annotations.items():
        if an in default_parameters:
            # Check if the type matches
            if typ != default_parameters[an].pytype:
                raise TypeError(
                    f"Type mismatch for parameter '{an}': "
                    f"expected {default_parameters[an].pytype}, got {typ}"
                )
            params[an] = namespace.get(an, None)
    return params


def _get_parameter_overrides(
    overrides: dict[str, int | float | str | None],
    default_parameters: dict[str, Parameter],
) -> dict[str, Parameter]:
    """Validate the class annotations against the default parameters and return the overridden parameters."""
    params: dict[str, Parameter] = {}
    for an, override in overrides.items():
        if an in default_parameters:
            # Check if the type matches
            if not isinstance(override, default_parameters[an].pytype):
                raise TypeError(
                    f"Type mismatch for parameter '{an}': "
                    f"expected {default_parameters[an].pytype}, got {type(override)}"
                )
            keyword = type(override).__name__
            signed = default_parameters[an].signed
            width = default_parameters[an].width
            value: int | str | float | None = override
            params[an] = Parameter(
                name=an,
                keyword=keyword,
                signed=signed,
                width=width,
                value=value,
            )
    return params


def _get_port_annotations(namespace) -> dict[str, Port]:
    """Check the port annotations against the model's ports and make sure they match."""
    annotations = _get_class_annotations(namespace)
    ports = {}
    for an, typ in annotations.items():
        if typ in _valid_port_types:
            # Type names look like InputU8 or OutputArrayS32.
            match = _PORT_TYPE_NAME.fullmatch(typ.__name__)
            assert match is not None
            width = int(match["width"])
            direction = match["direction"].lower()
            # Annotations don't specify extents. (0,) marks an array of unknown shape.
            shape = (0,) if match["array"] else ()

            ports[an] = Port(
                name=an,
                keyword="logic",
                signed=match["sign"] == "S",
                width=width,
                direction=direction,
                shape=shape,
            )
    return ports


def _validate_ports(ports: dict[str, Port], model_ports: dict[str, Port]) -> None:
    """Validate that the provided ports match the model's ports."""
    if len(ports) != len(model_ports):
        raise TypeError(
            "Partial port annotations are not allowed. "
            f"Annotated ports: {list(ports.keys())}, "
            f"Expected ports: {list(model_ports.keys())}"
        )
    for name, port in ports.items():
        if name not in model_ports:
            raise TypeError(f"Port '{name}' is not defined in the model")
        model_port = model_ports[name]
        if port.stdint_size != model_port.stdint_size:
            raise TypeError(
                f"Width mismatch for port '{name}': "
                f"expected {model_port.stdint_size}, got {port.stdint_size}"
            )
        if port.signed != model_port.is_signed_int:
            raise TypeError(
                f"Sign mismatch for port '{name}': "
                f"expected {'signed' if model_port.is_signed_int else 'unsigned'}, "
                f"got {'signed' if port.signed else 'unsigned'}"
            )
        if port.is_array != model_port.is_array:
            raise TypeError(
                f"Array mismatch for port '{name}': "
                f"expected {'an array' if model_port.is_array else 'a scalar'}, "
                f"got {'an array' if port.is_array else 'a scalar'}"
            )
        if port.direction != model_port.direction:
            raise TypeError(
                f"Direction mismatch for port '{name}': "
                f"expected {model_port.direction}, got {port.direction}"
            )


def _get_subdir_name(name: str, module_info: ModuleInfo) -> str:
    """Generated files are stored in a subdirectory of the cache directory based on the module name and parameters."""
    return f"{name}_{'-'.join(f'{k}={v.value}' for k, v in module_info.parameters.items())}_trace={module_info.trace}"


def _get_build_dir(name: str, module_info: ModuleInfo) -> Path:
    """Get the build directory for the module based on its name and parameters."""
    build_dir = cache_dir() / _get_subdir_name(name, module_info)
    return build_dir


def _compute_hash(generated_content: list[tuple[str, str]]) -> str:
    """Compute a hash for the generated content."""

    hasher = hashlib.sha256()
    for output_file, content in generated_content:
        hasher.update(output_file.encode())
        hasher.update(content.encode())
    return hasher.hexdigest()


def generate_files(file_pairs: list[tuple[str, str]], output_dir: Path, **kwargs):
    """
    Given a tuple of (output_file, template) pairs, generate the files in the build directory.
    Computes a hash and saves it in the build directory. If the hash matches the existing one, file generation is skipped.
    All kwargs are passed to all templates.
    """
    Path.mkdir(output_dir, parents=True, exist_ok=True)
    rendered_content = [
        (output_file, render_template(template, **kwargs))
        for output_file, template in file_pairs
    ]
    # Compute a hash and save it to the build_dir. If files haven't changed, skip generating.
    content_hash = _compute_hash(rendered_content)
    hash_file = output_dir / "__content_hash__.txt"
    if hash_file.exists():
        with open(hash_file, "r") as f:
            existing_hash = f.read().strip()
        if existing_hash == content_hash:
            return

    # Write the generated files to the build dir.
    for output_file, content in rendered_content:
        with open(output_dir / output_file, "w") as f:
            f.write(content)

        # Write the hash to a file in the build dir
        with open(hash_file, "w") as f:
            f.write(content_hash)


def generate_module_project_files(module_info: ModuleInfo, output_dir: Path) -> None:
    """Generate the project files for the module in the build directory."""

    # template, output_file
    gen_files = [
        (f"{module_info.name}.h", "verilator_module.h.jinja"),
        (f"{module_info.name}.cpp", "verilator_module.cpp.jinja"),
        (f"{module_info.name}_bind.h", "verilator_module_bind.h.jinja"),
        (f"{module_info.name}_module.cpp", "verilator_module_module.cpp.jinja"),
        ("CMakeLists.txt", "verilator_module_cmake.cmake.jinja"),
    ]
    generate_files(gen_files, output_dir, model=module_info)


def build_module(source_dir: Path, build_dir: Path, verbose: bool = False):
    """Build the module using CMake."""
    # build_dir = _get_build_dir(name, module_info)
    # build_dir
    # Search site-packages for CMake modules. nanobind and dspsim will be findable here.
    site_packages_path = sysconfig.get_paths()["purelib"]
    # Search if dspsim is an editable install?

    # Run the CMake configure command. Can be skipped if already configured, but it's not slow anyways.
    cmake_cfg_cmd = [
        "cmake",
        "-S",
        source_dir,
        "-B",
        build_dir,
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_PREFIX_PATH={site_packages_path}",
    ]
    out = subprocess.run(cmake_cfg_cmd, check=False, capture_output=True)
    if verbose:
        print(out.stdout.decode())
    if out.returncode != 0:
        print(f"Error occurred while running cmake: return code {out.returncode}")
        print(f"stderr: {out.stderr.decode()}")
        raise RuntimeError("CMake configuration failed")

    # Run the build command.
    cmake_build_cmd = [
        "cmake",
        "--build",
        build_dir,
        "--config",
        "Release",
    ]
    out = subprocess.run(cmake_build_cmd, check=False, capture_output=True)
    if verbose:
        print(out.stdout.decode())
    if out.returncode != 0:
        print(f"Error occurred while building module: return code {out.returncode}")
        print(f"stderr: {out.stderr.decode()}")
        raise RuntimeError("CMake build failed")


def _find_built_module(name: str, build_dir: Path) -> Path:
    """Find the built module's shared library file."""
    if sys.platform == "win32":
        suffix = ".pyd"
    else:
        suffix = ".so"

    # Might be in build or in build/Release depending on the platform/generator.
    return build_dir.glob(f"**/*_{name}*{suffix}").__iter__().__next__()


def _import_built_module(name: str, build_dir: Path):
    """Import the built module from its shared library file."""
    module_path = _find_built_module(name, build_dir)
    spec = importlib.util.spec_from_file_location(f"_{name}", module_path)
    if spec is None:
        raise ImportError(f"Could not load module spec for {module_path}")

    # Create a module based on the spec.
    module = importlib.util.module_from_spec(spec)

    # This can happen if it's a namespace package?
    if spec.loader is None:
        raise ImportError(f"Could not create module from spec for {module_path}")

    # Load the module.
    spec.loader.exec_module(module)

    return module


def build_vmodule(
    name: str,
    source: Path,
    include_dirs: list[Path] | None,
    parameters: dict[str, int | float | str | None] | None = None,
    trace: Literal["vcd", "fst", "platform"] | None = None,
    verbose: bool = False,
    *,
    # Optionally validate against a set of ports.
    ports: dict[str, Port] | None = None,
    # If default model info is provided, it will be used instead of reloading from the source.
    default_model_info: ModuleInfo | None = None,
):
    """Build and return the verilated model class."""
    # Load the verilated model's default parameters.
    if default_model_info is None:
        default_model_info = load_model_info(
            source, include_dirs=include_dirs, parameters={}
        )

    # Validate the parameter annotations, and create a set of parameter overrides.
    parameter_overrides = _get_parameter_overrides(
        parameters or {}, default_model_info.parameters
    )
    # Re-generate the model with the parameter annotations applied.
    # Port sizes may change with parameter overrides.
    module_info = load_model_info(
        source, include_dirs=include_dirs, parameters=parameter_overrides
    )

    # Validate ports if annotations were supplied.
    if ports:
        _validate_ports(ports, module_info.ports)

    # At this point, the annotations are valid.

    # Use the specified name for the module.
    module_info.name = name

    # Configure tracing.
    if trace == "platform":
        if sys.platform == "win32":
            trace = "vcd"
        else:
            trace = "fst"
    module_info.trace = trace

    # Build the class.
    output_dir = _get_build_dir(module_info.name, module_info)
    build_dir = output_dir / "build"

    # Generate project files
    generate_module_project_files(module_info, output_dir)

    # Build the module!
    build_module(output_dir, build_dir, verbose=verbose)

    # Import the built module.
    module = _import_built_module(module_info.name, build_dir)

    # Link the module's context factory to the main dspsim context factory.
    dspsim.link_module(module)

    # Get the Module class from the imported module.
    module_cls = getattr(module, module_info.name)

    # Add the module_info as metadata? wrapper?
    # Nanobind modules don't support adding arbitrary attributes to the class, so we can't attach module_info directly.
    # Could subclass the nanobind module and add attributes.
    return module_cls


# Tell the type checker about VModule classes.
if TYPE_CHECKING:

    class VModule(_Module):
        def __init__(self, name: str): ...
        def open_trace(self, trace_path: Path, levels: int = 99, options: int = 0): ...
        def close_trace(self): ...

else:
    VModule = object


def vbuilder(
    source: Path,
    include_dirs: list[Path] | None = None,
    trace: Literal["vcd", "fst", "platform"] | None = None,
    verbose: bool = False,
):
    """
    Generate a verilator model to use with dspsim. Parameters can be overridden via class annotations.
    Ports may also be annotated to give a type hint for what the model has. If port annotations are given,
    they must match the model exactly.
    """

    def class_decorator[V: type[VModule]](cls: V) -> V:
        # Load the verilated model's default parameters.
        default_model_info = load_model_info(
            source, include_dirs=include_dirs, parameters={}
        )

        # Check the class annotations for valid parameters.
        parameter_annotations = _get_parameter_annotations(
            cls.__dict__, default_model_info.parameters
        )

        # Get port annotations
        port_annotations = _get_port_annotations(cls.__dict__)

        # build the module
        wrapper = build_vmodule(
            cls.__name__,
            source,
            include_dirs=include_dirs,
            parameters=parameter_annotations,
            trace=trace,
            verbose=verbose,
            ports=port_annotations,
            default_model_info=default_model_info,
        )  # type: ignore
        wrapper: V
        return wrapper

    return class_decorator
