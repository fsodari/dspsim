import annotationlib
import atexit
import functools
import glob
import hashlib
import importlib.util
import os
import subprocess
import sys
import sysconfig
from itertools import groupby
from pathlib import Path
from typing import Any, Literal

import dotenv

from dspsim.framework import (
    Input8,
    Input16,
    Input32,
    Input64,
    ModuleName,
    Output8,
    Output16,
    Output32,
    Output64,
    _Module,
    get_global_context_factory,
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
    Input8,
    Input16,
    Input32,
    Input64,
    Output8,
    Output16,
    Output32,
    Output64,
]


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
            _direction, _width = (
                "".join(group) for key, group in groupby(typ.__name__, key=str.isdigit)
            )
            width = int(_width)
            direction = _direction.lower()

            ports[an] = Port(
                name=an,
                keyword="logic",
                signed=False,
                width=width,
                direction=direction,
                shape=(),
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


def _compute_hash(generated_content: list[tuple[Path, str]]) -> str:
    """Compute a hash for the generated content."""

    hasher = hashlib.sha256()
    for output_file, content in generated_content:
        hasher.update(str(output_file.absolute()).encode())
        hasher.update(content.encode())
    return hasher.hexdigest()


def generate_project_files(name: str, module_info: ModuleInfo) -> None:
    """Generate the project files for the module in the build directory."""
    build_dir = _get_build_dir(name, module_info)
    Path.mkdir(build_dir, parents=True, exist_ok=True)

    # template, output_file
    gen_files = [
        (build_dir / f"{name}.h", "verilator_module.h.jinja"),
        (build_dir / f"{name}.cpp", "verilator_module.cpp.jinja"),
        (build_dir / f"{name}_bind.h", "verilator_module_bind.h.jinja"),
        (build_dir / f"{name}_module.cpp", "verilator_module_module.cpp.jinja"),
        (build_dir / "CMakeLists.txt", "verilator_module_cmake.cmake.jinja"),
    ]
    # Render the templates.
    rendered_content = [
        (output_file, render_template(template, model=module_info))
        for output_file, template in gen_files
    ]

    # Compute a hash and save it to the build_dir. If files haven't changed, skip generating.
    content_hash = _compute_hash(rendered_content)
    hash_file = build_dir / "content_hash.txt"
    if hash_file.exists():
        with open(hash_file, "r") as f:
            existing_hash = f.read().strip()
        if existing_hash == content_hash:
            return

    # Write the hash to a file in the build dir
    with open(hash_file, "w") as f:
        f.write(content_hash)

    # Write the generated files to the build dir.
    for output_file, content in rendered_content:
        with open(output_file, "w") as f:
            f.write(content)


def build_module(name: str, module_info: ModuleInfo, verbose: bool = False):
    """Build the module using CMake."""
    build_dir = _get_build_dir(name, module_info)
    # Search site-packages for CMake modules. nanobind and dspsim will be findable here.
    site_packages_path = sysconfig.get_paths()["purelib"]
    # Search if dspsim is an editable install?

    # Run the CMake configure command. Can be skipped if already configured, but it's not slow anyways.
    cmake_cfg_cmd = [
        "cmake",
        "-S",
        build_dir,
        "-B",
        build_dir / "build",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_PREFIX_PATH={site_packages_path}",
    ]
    out = subprocess.run(cmake_cfg_cmd, check=True, capture_output=True)
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
        build_dir / "build",
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


def _find_built_module(name: str, module_info: ModuleInfo) -> Path:
    """Find the built module's shared library file."""
    if sys.platform == "win32":
        suffix = ".pyd"
    else:
        suffix = ".so"

    return Path(
        glob.glob(str(_get_build_dir(name, module_info) / "build" / f"**/_{name}*{suffix}"))[0]
    )


def _import_built_module(name: str, module_info: ModuleInfo):
    """Import the built module from its shared library file."""
    module_path = _find_built_module(name, module_info)
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

    # Validate the overrides and update them.
    parameter_overrides = _get_parameter_overrides(
        parameters or {}, default_model_info.parameters
    )
    # Re-generate the model with the parameter annotations applied. Port sizes may change.
    module_info = load_model_info(
        source, include_dirs=include_dirs, parameters=parameter_overrides
    )

    # Validate ports.
    if ports:
        _validate_ports(ports, module_info.ports)

    # At this point, the annotations check out.
    # Build the class.
    module_info.name = name
    if trace == "platform":
        if sys.platform == "win32":
            trace = "vcd"
        else:
            trace = "fst"
    module_info.trace = trace

    # Generate project files
    generate_project_files(module_info.name, module_info)

    # Build the module!
    build_module(module_info.name, module_info, verbose=verbose)

    # Import the built module.
    module = _import_built_module(module_info.name, module_info)

    # Link the module's context factory to the main dspsim context factory.
    module.set_global_context_factory(get_global_context_factory())
    atexit.register(module.reset_global_context_factory)

    # Get the Module class from the imported module.
    module_cls = getattr(module, f"{module_info.name}")

    # Wrapper to get an instance of the class
    @functools.wraps(module_cls)
    def wrapper(name: str):
        # Create a module name object before instantiating the module class.
        _name = ModuleName(name)
        # Replace the class.
        instance = module_cls(_name)
        # Delete the ModuleName to trigger the _end_construction method in the module class.
        del _name
        return instance

    # This works for a type hint, but I still get a type hint error if I don't ignore.
    return wrapper  # type: ignore


class VModule(_Module):
    def __init__(self, name: str): ...
    def open_trace(self, trace_path: Path, levels: int = 99, options: int = 0): ...
    def close_trace(self): ...


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
