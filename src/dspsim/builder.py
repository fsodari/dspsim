import annotationlib
import dataclasses
import hashlib
import importlib.metadata
import importlib.util
import os
import re
import subprocess
import sys
import sysconfig
import warnings
from collections.abc import Sequence
from pathlib import Path
from typing import TYPE_CHECKING, Any, Literal

import dotenv

import dspsim
from dspsim.cpp_parser import CppHeader
from dspsim.framework import (
    InputArrayFloat,
    InputArrayS8,
    InputArrayS16,
    InputArrayS32,
    InputArrayS64,
    InputArrayU8,
    InputArrayU16,
    InputArrayU32,
    InputArrayU64,
    InputFloat,
    InputS8,
    InputS16,
    InputS32,
    InputS64,
    InputU8,
    InputU16,
    InputU32,
    InputU64,
    OutputArrayFloat,
    OutputArrayS8,
    OutputArrayS16,
    OutputArrayS32,
    OutputArrayS64,
    OutputArrayU8,
    OutputArrayU16,
    OutputArrayU32,
    OutputArrayU64,
    OutputFloat,
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
from dspsim.module_info import (
    CppModuleInfo,
    CppPort,
    CppSkipped,
    CppTemplateParameter,
    ModuleInfo,
    Parameter,
    Port,
)
from dspsim.project import load_model_info
from dspsim.templates import render_template
from dspsim.toolchain import (
    build_key,
    cmake_executable,
    dependency_compile_args,
    toolchain,
)

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


# C++ modules can also have ports of floating point values.
_valid_cpp_port_types = [
    *_valid_port_types,
    InputFloat,
    InputArrayFloat,
    OutputFloat,
    OutputArrayFloat,
]


def _get_cpp_port_annotations(namespace: dict[str, Any]) -> dict[str, type]:
    """The class annotations that are port types, by port name."""
    annotations = _get_class_annotations(namespace)
    return {an: typ for an, typ in annotations.items() if typ in _valid_cpp_port_types}


def _get_cpp_parameter_annotations(
    namespace: dict[str, Any], template_parameters: dict[str, CppTemplateParameter]
) -> dict[str, Any]:
    """The class annotations that name template parameters, with the values assigned to them."""
    annotations = _get_class_annotations(namespace)
    params: dict[str, Any] = {}
    for an, typ in annotations.items():
        if an in template_parameters:
            pytypes = template_parameters[an].pytypes
            if not pytypes:
                raise TypeError(f"Template parameter '{an}' cannot be set from Python")
            if typ not in pytypes:
                expected = " or ".join(t.__name__ for t in pytypes)
                raise TypeError(
                    f"Type mismatch for parameter '{an}': expected {expected}, got {typ}"
                )
            params[an] = namespace.get(an, None)
    return params


def _validate_cpp_ports(
    ports: dict[str, type], model_ports: dict[str, CppPort]
) -> None:
    """Validate that the annotated port types are the ports of the C++ module."""
    if set(ports) != set(model_ports):
        if undefined := [name for name in ports if name not in model_ports]:
            raise TypeError(f"Port '{undefined[0]}' is not defined in the model")
        raise TypeError(
            "Partial port annotations are not allowed. "
            f"Annotated ports: {list(ports.keys())}, "
            f"Expected ports: {list(model_ports.keys())}"
        )
    for name, typ in ports.items():
        model_port = model_ports[name]
        expected = model_port.framework_type
        if expected is None:
            raise TypeError(
                f"Port '{name}' has no Python type: "
                f"dspsim has no bindings for ports of '{model_port.ctype}'"
            )
        if typ.__name__ != expected:
            raise TypeError(
                f"Type mismatch for port '{name}': expected {expected}, got {typ.__name__}"
            )


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

    # Write the hash to a file in the build dir, once every file is written.
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


def build_module(
    source_dir: Path, build_dir: Path, verbose: bool = False, configure: bool = True
):
    """
    Build the module using CMake. Without `configure`, only the build step is run, for a build directory that
    is configured already: the build system then checks what the module depends on and rebuilds what is out
    of date.
    """
    # Search site-packages for CMake modules. nanobind and dspsim will be findable here.
    site_packages_path = sysconfig.get_paths()["purelib"]

    if configure:
        cmake_cfg_cmd = [
            cmake_executable(),
            "-S",
            source_dir,
            "-B",
            build_dir,
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_PREFIX_PATH={site_packages_path}",
            # The interpreter that is running, which has dspsim: CMake would otherwise take the first Python
            # that it finds, and build for that one and run the dspsim generator of a library with it.
            f"-DPython_EXECUTABLE={Path(sys.executable).as_posix()}",
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
        cmake_executable(),
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


def _build(output_dir: Path, build_dir: Path, verbose: bool):
    """
    Build the module in `output_dir`, or bring it up to date.

    The build step always runs, because only the build system knows everything that the module depends on:
    it records every header that the compiler read, those of the system and of other libraries included, and
    it rebuilds the module when one has changed. For a module that is up to date that takes a few
    milliseconds. The configure step is slower and runs once, and again when the generated files or the
    toolchain have changed, which the stamp tells.
    """
    stamp_file = build_dir / "__build_stamp__.txt"
    stamp = (output_dir / "__content_hash__.txt").read_text() + "\n" + toolchain()
    is_configured = stamp_file.exists() and stamp_file.read_text() == stamp
    build_module(output_dir, build_dir, verbose=verbose, configure=not is_configured)
    stamp_file.write_text(stamp)


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

    # Load the module. It is registered like an imported module, so that the __module__ of its classes
    # names a module: a build has a name of its own.
    sys.modules[spec.name] = module
    try:
        spec.loader.exec_module(module)
    except BaseException:
        del sys.modules[spec.name]
        raise

    return module


# The extension modules imported in this process, by file, with the modification time of the file then.
_loaded_modules: dict[Path, tuple[int, Any]] = {}


def _load_built_module(build_name: str, build_dir: Path, name: str):
    """
    Import the extension module of a build and link it to the framework. `name` is the class in it.
    Building the same module again in the same process gives the module that is already imported: an
    extension module is only loaded once.
    """
    module_path = _find_built_module(build_name, build_dir).resolve()
    modified = module_path.stat().st_mtime_ns
    if module_path in _loaded_modules:
        loaded_modified, module = _loaded_modules[module_path]
        if loaded_modified != modified:
            # The build key covers the sources and the options, so this is something else that the module
            # depends on: a header of the system or of a library, dspsim, the compiler.
            raise RuntimeError(
                f"{name} was built again after it was imported, because something that it depends on "
                "has changed (a header of the system or of a library, dspsim, the compiler). "
                "Restart Python to load the new build."
            )
        return module

    module = _import_built_module(build_name, build_dir)
    # A C++ class has one Python class per process. If another module bound it first, this one has none.
    # Builds have a C++ class of their own, except those of a C++ module that is a final class.
    if not hasattr(module, name):
        raise RuntimeError(
            f"{name}: its C++ class is already bound to another Python class in this process"
        )

    # Link the module's context factory to the main dspsim context factory.
    dspsim.link_module(module)
    _loaded_modules[module_path] = (modified, module)
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

    # Configure tracing.
    if trace == "platform":
        if sys.platform == "win32":
            trace = "vcd"
        else:
            trace = "fst"
    module_info.trace = trace

    # The Python class has the specified name. What is generated for it (the C++ class, the verilated model,
    # the extension module) is named after this build, so that other builds of the same name can coexist.
    module_info.python_name = name
    description = {
        "name": name,
        "module": dataclasses.asdict(module_info),
        "include_dirs": [Path(d).resolve() for d in include_dirs or []],
    }
    sources = module_info.source_files or [source]
    module_info.name = f"{name}_{build_key(description, sources)}"

    # Build the class.
    output_dir = cache_dir() / module_info.name
    build_dir = output_dir / "build"

    # Generate project files
    generate_module_project_files(module_info, output_dir)

    # Build the module, or check that it is up to date.
    _build(output_dir, build_dir, verbose)

    # Import the built module and link it to the framework.
    module = _load_built_module(module_info.name, build_dir, name)

    # Get the Module class from the imported module.
    module_cls = getattr(module, name)

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


def generate_cpp_module_project_files(
    module_info: CppModuleInfo, output_dir: Path
) -> None:
    """Generate the project files for a C++ module in the build directory."""
    gen_files = [
        (f"{module_info.name}_bind.h", "cpp_module_bind.h.jinja"),
        (f"{module_info.name}_module.cpp", "cpp_module_module.cpp.jinja"),
        ("CMakeLists.txt", "cpp_module_cmake.cmake.jinja"),
    ]
    generate_files(gen_files, output_dir, model=module_info)


def _parse_cpp_header(
    header: Path,
    include_dirs: list[Path] | None,
    definitions: dict[str, Any] | None,
    sources: list[Path] | None,
    packages: Sequence[str] | None,
    libraries: Sequence[str] | None,
    verbose: bool,
) -> CppHeader:
    """Parse the header of a C++ module with what the module is built with."""
    compile_args = dependency_compile_args(
        packages or [], libraries or [], cache_dir(), verbose
    )
    return CppHeader.load(header, include_dirs, definitions, sources, compile_args)


class CppBindingWarning(UserWarning):
    """A public constructor or method of a C++ module is not bound."""


def _report_skipped(name: str, skipped: list[CppSkipped], strict: bool) -> None:
    """Warn about the constructors and methods that are not bound, or raise if strict."""
    if not skipped:
        return
    messages = [f"'{s.signature}' is not bound: {s.reason}" for s in skipped]
    if strict:
        raise TypeError(f"C++ module {name}:\n  " + "\n  ".join(messages))
    for message in messages:
        # The warning points at the code that builds the module, not into dspsim.
        warnings.warn(
            f"C++ module {name}: {message}",
            CppBindingWarning,
            skip_file_prefixes=(str(Path(__file__).parent),),
        )


def build_cppmodule(
    name: str,
    header: Path,
    cls: str | None = None,
    include_dirs: list[Path] | None = None,
    parameters: dict[str, Any] | None = None,
    methods: bool | Sequence[str] = True,
    strict: bool = False,
    verbose: bool = False,
    *,
    sources: list[Path] | None = None,
    definitions: dict[str, Any] | None = None,
    packages: Sequence[str] | None = None,
    libraries: Sequence[str] | None = None,
    bindings: Path | None = None,
    # Optionally validate against a set of port types.
    ports: dict[str, type] | None = None,
    # If the parsed header is provided, it will be used instead of parsing the header again.
    parsed_header: CppHeader | None = None,
):
    """
    Build the bindings of a C++ dspsim Module and return its Python class, called `name`.

    `cls` is the name of the C++ class in the header, with or without its namespace. It defaults to `name`.
    `parameters` are the template arguments of a class template, by parameter name: a numpy type or the name
    of a C++ type for a type parameter, an int or a bool for the others.
    `methods` selects the public methods to bind: all of them (True), none (False) or those named.
    A constructor or method that cannot be bound is left out with a CppBindingWarning, or a TypeError if
    `strict`.

    What the module is built with besides the header:
    - `include_dirs`: include directories.
    - `sources`: source files that define what the header declares. They are compiled into the module.
    - `definitions`: compile definitions, {"NAME": value} or {"NAME": None}.
    - `packages`: CMake packages, each the arguments of a find_package(): "Eigen3", "Boost COMPONENTS regex".
    - `libraries`: what to link: the targets of the packages ("Eigen3::Eigen") or libraries.
    - `bindings`: a header with bindings of your own. It defines
      `template <typename M> void dspsim_bind(nanobind::module_ &m, nanobind::class_<M, dspsim::Module> &cls)`,
      which is called before the constructors and methods are bound: it can bind the classes that they
      use, and add members to `cls`.
    """
    if parsed_header is None:
        parsed_header = _parse_cpp_header(
            header, include_dirs, definitions, sources, packages, libraries, verbose
        )
    module_info = parsed_header.module_info(cls or name, parameters, methods)
    module_info.packages = list(packages or [])
    module_info.libraries = list(libraries or [])
    if bindings is not None:
        module_info.bindings = Path(bindings).resolve()
        if not module_info.bindings.is_file():
            raise FileNotFoundError(f"C++ bindings header not found: {bindings}")

    # Validate ports if annotations were supplied.
    if ports:
        _validate_cpp_ports(ports, module_info.ports)

    # Use the specified name for the Python class.
    module_info.name = name

    # With strict, what the parser already knows cannot be bound fails before the build.
    if strict:
        _report_skipped(name, module_info.skipped, strict)

    # The extension module and the C++ class of the bindings are named after this build: the description
    # has everything that the generated code depends on, and the header is hashed with what it includes.
    source_files = parsed_header.source_files()
    if module_info.bindings is not None:
        source_files.append(module_info.bindings)
    key = build_key(dataclasses.asdict(module_info), source_files)
    module_info.build_name = f"{name}_{key}"

    output_dir = cache_dir() / module_info.build_name
    build_dir = output_dir / "build"

    generate_cpp_module_project_files(module_info, output_dir)
    _build(output_dir, build_dir, verbose)

    module = _load_built_module(module_info.build_name, build_dir, name)

    # The module lists what it left out because a class has no Python bindings: that is only known once
    # the framework and the module are loaded.
    functions = {f.signature: f for f in module_info.constructors + module_info.methods}
    unbound = [
        CppSkipped(
            signature,
            f"no Python bindings for {' or '.join(functions[signature].classes)}",
        )
        for _, signature in module._unbound
    ]
    _report_skipped(name, module_info.skipped + unbound, strict)

    return getattr(module, module_info.name)


# Tell the type checker about C++ module classes.
if TYPE_CHECKING:

    class CppModule(_Module):
        def __init__(self, name: str, *args: Any, **kwargs: Any): ...

else:
    CppModule = object


def cppbuilder(
    header: Path,
    cls: str | None = None,
    include_dirs: list[Path] | None = None,
    methods: bool | Sequence[str] = True,
    strict: bool = False,
    verbose: bool = False,
    *,
    sources: list[Path] | None = None,
    definitions: dict[str, Any] | None = None,
    packages: Sequence[str] | None = None,
    libraries: Sequence[str] | None = None,
    bindings: Path | None = None,
):
    """
    Generate the bindings of a C++ dspsim Module to use it from Python. `cls` names the C++ class in the
    header (default: the name of the decorated class). The template parameters of a class template are set
    with class annotations: `T: type = np.int16` (or `T: str = "demo::Sample"`), `SHIFT: int = 3`.
    Ports may also be annotated to give a type hint for what the module has. If port annotations are given,
    they must match the module exactly.

    The public constructors that take a ModuleName first and the public methods are bound. `methods` selects
    the methods: all of them (True), none (False) or those named. What cannot be bound is left out with a
    CppBindingWarning, or a TypeError if `strict`.

    `sources`, `definitions`, `packages`, `libraries` and `bindings` are what the module is built with
    besides the header and `include_dirs`: see build_cppmodule().
    """

    def class_decorator[M: type[CppModule]](cls_: M) -> M:
        parsed_header = _parse_cpp_header(
            header, include_dirs, definitions, sources, packages, libraries, verbose
        )
        cpp_cls = cls or cls_.__name__

        # Check the class annotations for template parameters.
        parameter_annotations = _get_cpp_parameter_annotations(
            cls_.__dict__, parsed_header.template_parameters(cpp_cls)
        )

        wrapper = build_cppmodule(
            cls_.__name__,
            header,
            cls=cpp_cls,
            include_dirs=include_dirs,
            parameters=parameter_annotations,
            methods=methods,
            strict=strict,
            verbose=verbose,
            packages=packages,
            libraries=libraries,
            bindings=bindings,
            ports=_get_cpp_port_annotations(cls_.__dict__),
            parsed_header=parsed_header,
        )  # type: ignore
        wrapper: M
        return wrapper

    return class_decorator
