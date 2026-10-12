"""Project configuration. Configuration is given in a .toml file. Typically in the pyproject.toml file"""

import dataclasses
import glob
import tomllib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Literal

from .module_info import CppModuleInfo, ModuleInfo, Parameter
from .verilator import parse_module_json, verilate_json


def load_model_info(
    source: Path,
    include_dirs: list[Path] | None = None,
    parameters: dict[str, Parameter] | None = None,
) -> ModuleInfo:
    """Run verilator to glean the default information about a model, specifically its parameters."""
    if include_dirs is None:
        include_dirs = []
    if parameters is None:
        parameters = {}

    param_values = {k: p.value for k, p in parameters.items() if p.value is not None}
    model_data, metadata = verilate_json(
        [source],
        include_dirs=include_dirs,
        parameters=param_values,
    )
    return parse_module_json(model_data, metadata)


@dataclass
class CppProjectConfig:
    """
    The C++ modules of a library, from [tool.dspsim.cpp]. They are bound like dspsim.builder.cppbuilder binds
    them (docs/CppBuilder.md), all in the one extension module of the library.

    The modules are compiled in one target, so what they are built with is common to all of them: include
    directories, compile definitions, source files, CMake packages, libraries and the bindings header.
    """

    include_dirs: list[Path] = field(default_factory=list)
    sources: list[Path] = field(default_factory=list)
    definitions: dict[str, str | None] = field(default_factory=dict)
    packages: list[str] = field(default_factory=list)
    libraries: list[str] = field(default_factory=list)
    bindings: Path | None = None
    # Fail the build if a public constructor or method of a module cannot be bound.
    strict: bool = False
    # [tool.dspsim.cpp.models.<name>]: the Python class name of each module, with its description.
    models: dict[str, CppModuleInfo] = field(default_factory=dict)
    # The files that the generated bindings depend on: the headers and what they include.
    source_files: list[Path] = field(default_factory=list)

    @classmethod
    def from_config(
        cls, config: dict[str, Any], root: Path, library: str, output_dir: Path
    ):
        """
        Read [tool.dspsim.cpp] and parse the headers of its models. Paths are relative to `root`, the
        directory of the pyproject.toml. `output_dir` is where CMake is asked about the packages.
        """
        # Only a library with C++ modules needs libclang.
        from .cpp_parser import CppHeader
        from .toolchain import build_key, dependency_compile_args

        def paths(patterns: list[str]) -> list[Path]:
            found = [g for p in patterns for g in sorted(glob.glob(p, root_dir=root))]
            return [(root / g).resolve() for g in found]

        self = cls(
            include_dirs=paths(config.get("include_dirs", [])),
            sources=paths(config.get("sources", [])),
            # NAME = true defines NAME without a value.
            definitions={
                name: None if value is True else str(value)
                for name, value in config.get("definitions", {}).items()
            },
            packages=list(config.get("packages", [])),
            libraries=list(config.get("libraries", [])),
            strict=bool(config.get("strict", False)),
        )
        if "bindings" in config:
            bindings = (root / config["bindings"]).resolve()
            if not bindings.is_file():
                raise FileNotFoundError(f"C++ bindings header not found: {bindings}")
            self.bindings = bindings

        compile_args = dependency_compile_args(
            self.packages, self.libraries, output_dir
        )
        source_files = set(self.sources)
        if self.bindings is not None:
            source_files.add(self.bindings)

        for name, model_config in config.get("models", {}).items():
            parsed = CppHeader.load(
                root / model_config["header"],
                self.include_dirs,
                self.definitions,
                self.sources,
                compile_args,
            )
            info = parsed.module_info(
                model_config.get("cls", name),
                model_config.get("parameters"),
                model_config.get("methods", True),
            )
            info.name = name
            info.packages = self.packages
            info.libraries = self.libraries
            info.bindings = self.bindings
            # The C++ class of the bindings has a name of its own, as it has for a module that
            # dspsim.builder builds: two libraries can bind the same C++ class.
            key = build_key(dataclasses.asdict(info), parsed.source_files())
            info.build_name = f"{library}_{name}_{key}"
            self.models[name] = info
            source_files.update(parsed.source_files())

        self.source_files = sorted(source_files)
        return self


@dataclass
class ProjectConfig:
    name: str
    sources: list[Path]
    exclude_sources: list[Path]
    include_dirs: list[Path]
    parameters: dict[str, Parameter]
    trace: Literal["vcd", "fst"] | None
    models: dict[str, ModuleInfo]
    # The C++ modules of the library.
    cpp: CppProjectConfig = field(default_factory=CppProjectConfig)

    @classmethod
    def from_toml(cls, source: Path, output_dir: Path | None = None):
        """
        Parse project configuration from a TOML file. Relative paths in it are relative to its directory.
        `output_dir` is the directory that files are generated in: see CppProjectConfig.from_config().
        """
        source = Path(source).resolve()
        root = source.parent
        with open(source, "rb") as f:
            config = tomllib.load(f)["tool"]["dspsim"]

        def paths(patterns: list[str]) -> list[Path]:
            return [
                root / g for p in patterns for g in sorted(glob.glob(p, root_dir=root))
            ]

        globbed_sources = paths(config.get("sources", []))
        # remove exclude sources from global sources
        exclude_sources = paths(config.get("exclude_sources", []))
        globbed_sources = [s for s in globbed_sources if s not in exclude_sources]
        _global_include_dirs = paths(config.get("include_dirs", []))

        _global_parameters = {
            k: Parameter(k, "", False, -1, v)
            for k, v in config.get("parameters", {}).items()
        }
        _global_trace = config.get("trace", None)

        # Populate default models
        default_models: dict[str, ModuleInfo] = {}
        for s in globbed_sources:
            default_model = load_model_info(s, _global_include_dirs)
            param_overrides = {
                k: v
                for k, v in _global_parameters.items()
                if k in default_model.parameters
            }
            default_model = load_model_info(s, _global_include_dirs, param_overrides)
            default_model.trace = _global_trace
            default_models[default_model.name] = default_model

        # Go through extra/override models
        for model_name, model_config in config.get("models", {}).items():
            model_includes = _global_include_dirs + [
                Path(p) for p in model_config.get("include_dirs", [])
            ]

            if model_name in default_models:
                # Apply parameter overrides
                for k, v in model_config.get("parameters", {}).items():
                    default_models[model_name].parameters[k] = Parameter(
                        k, "", False, -1, v
                    )

                # Reload module with parameter overrides.
                default_models[model_name] = load_model_info(
                    Path(default_models[model_name].source),
                    model_includes,
                    default_models[model_name].parameters,
                )
                # Apply trace override if specified in the model configuration.
                default_models[model_name].trace = _global_trace
                if "trace" in model_config:
                    default_models[model_name].trace = model_config["trace"]

            else:
                # Add new model if it doesn't exist in default models.
                default_model = load_model_info(
                    Path(model_config["source"]),
                    model_includes,
                )

                # Apply parameter overrides
                for k, p in _global_parameters.items():
                    if k in default_model.parameters:
                        default_model.parameters[k].value = p.value
                for k, v in model_config.get("parameters", {}).items():
                    if k not in default_model.parameters:
                        raise KeyError(
                            f"Parameter '{k}' not found in model '{default_model.name}'"
                        )
                    default_model.parameters[k].value = v

                # Reload model
                default_model = load_model_info(
                    Path(model_config["source"]),
                    model_includes,
                    default_model.parameters,
                )
                default_model.name = model_name
                default_model.trace = _global_trace
                if "trace" in model_config:
                    default_model.trace = model_config["trace"]
                default_models[default_model.name] = default_model
        name = config.get("name", "")
        cpp = CppProjectConfig()
        if "cpp" in config:
            for model_name in config["cpp"].get("models", {}):
                if model_name in default_models:
                    raise ValueError(
                        f"'{model_name}' is the name of a C++ module and of a Verilog model"
                    )
            cpp = CppProjectConfig.from_config(
                config["cpp"], root, name, output_dir or root / ".dspsim_cache"
            )

        return cls(
            name=name,
            sources=globbed_sources,
            exclude_sources=exclude_sources,
            include_dirs=_global_include_dirs,
            parameters=_global_parameters,
            trace=_global_trace,
            models=default_models,
            cpp=cpp,
        )

    def report(self) -> str:
        """Print a nicely formatted string of all the project configuration."""
        report_lines = [
            f"Project Name: {self.name}",
            f"Sources:\n  {'\n  '.join(str(s) for s in self.sources)}",
            f"Include Dirs:\n  {'\n  '.join(str(d) for d in self.include_dirs)}",
            f"Global Trace: {self.trace}",
            "Global Parameters:",
        ]
        for k, v in self.parameters.items():
            report_lines.append(f"  {k}: {v}")
        report_lines.append("Models:")
        for model_name, model in self.models.items():
            report_lines.append(f"  {model_name}:")
            report_lines.append(f"    Name: {model.name}")
            report_lines.append(f"    Source: {model.source}")
            report_lines.append(f"    Trace: {model.trace}")
            report_lines.append("    Parameters:")
            for k, v in model.parameters.items():
                report_lines.append(f"      {k}: {v}")
            report_lines.append("    Ports:")
            for k, v in model.ports.items():
                report_lines.append(f"      {k}: {v}")
        report_lines.append("C++ Models:")
        for model_name, model in self.cpp.models.items():
            report_lines.append(f"  {model_name}: {model.cpp_type} ({model.header})")
            report_lines.append(f"    Ports: {', '.join(model.ports)}")
            report_lines.append(
                f"    Methods: {', '.join(m.signature for m in model.methods)}"
            )
        return "\n".join(report_lines)
