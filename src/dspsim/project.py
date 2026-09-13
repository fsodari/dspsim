"""Project configuration. Configuration is given in a .toml file. Typically in the pyproject.toml file"""

import glob
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Literal

import dspsim

from .module_info import ModuleInfo, Parameter
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

    param_values = {k: p.value for k, p in parameters.items()}
    model_data, metadata = verilate_json(
        [source],
        include_dirs=include_dirs,
        parameters=param_values,
    )
    return parse_module_json(model_data, metadata)


@dataclass
class ProjectConfig:
    name: str
    include_dspsim_library: bool
    sources: list[Path]
    exclude_sources: list[Path]
    include_dirs: list[Path]
    parameters: dict[str, Parameter]
    trace: Literal["vcd", "fst"] | None
    models: dict[str, ModuleInfo]

    @classmethod
    def from_toml(cls, source: Path):
        """Parse project configuration from a TOML file."""
        with open(source, "rb") as f:
            config = tomllib.load(f)["tool"]["dspsim"]

        globbed_sources = [
            Path(g) for s in config.get("sources", []) for g in glob.glob(s)
        ]
        # remove exclude sources from global sources
        exclude_sources = [
            Path(g) for s in config.get("exclude_sources", []) for g in glob.glob(s)
        ]
        globbed_sources = [s for s in globbed_sources if s not in exclude_sources]
        _global_include_dirs = [
            Path(g) for i in config.get("include_dirs", []) for g in glob.glob(i)
        ]
        if config.get("include_dspsim_library", False):
            _global_include_dirs.append(dspsim.hdl_dir())
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
        return cls(
            name=config.get("name", ""),
            sources=globbed_sources,
            exclude_sources=exclude_sources,
            include_dirs=_global_include_dirs,
            parameters=_global_parameters,
            trace=_global_trace,
            include_dspsim_library=config.get("include_dspsim_library", False),
            models=default_models,
        )

    def report(self) -> str:
        """Print a nicely formatted string of all the project configuration."""
        report_lines = [
            f"Project Name: {self.name}",
            f"Include DSPSim Library: {self.include_dspsim_library}",
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
        return "\n".join(report_lines)
