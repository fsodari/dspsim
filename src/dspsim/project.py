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
    include_dirs: list[Path]
    parameters: dict[str, Parameter]
    trace: Literal["vcd", "fst"] | None
    models: dict[str, ModuleInfo]

    @classmethod
    def from_toml(cls, source: Path):
        """Parse project configuration from a TOML file."""
        with open(source, "rb") as f:
            config = tomllib.load(f)["tool"]["dspsim"]

        global_sources = [
            Path(g) for s in config.get("sources", []) for g in glob.glob(s)
        ]
        global_include_dirs = [
            Path(g) for i in config.get("include_dirs", []) for g in glob.glob(i)
        ]
        if config.get("include_dspsim_library", False):
            global_include_dirs.append(dspsim.hdl_dir())
        global_parameters = {
            k: Parameter(k, "", False, v)
            for k, v in config.get("parameters", {}).items()
        }
        global_trace = config.get("trace", None)

        # Populate default models
        default_models: dict[str, ModuleInfo] = {}
        for s in global_sources:
            default_model = load_model_info(s, global_include_dirs)
            param_overrides = {
                k: v
                for k, v in global_parameters.items()
                if k in default_model.parameters
            }
            default_model = load_model_info(s, global_include_dirs, param_overrides)
            default_model.trace = global_trace
            default_models[default_model.name] = default_model

        # Go through extra/override models
        for model_config in config.get("models", []):
            model_includes = global_include_dirs + [
                Path(p) for p in model_config.get("include_dirs", [])
            ]

            if model_config["name"] in default_models:
                # Apply parameter overrides
                for k, v in model_config.get("parameters", {}).items():
                    default_models[model_config["name"]].parameters[k] = Parameter(
                        k, "", False, v
                    )

                # Reload module with parameter overrides.
                default_models[model_config["name"]] = load_model_info(
                    Path(default_models[model_config["name"]].source),
                    model_includes,
                    default_models[model_config["name"]].parameters,
                )
                # Apply trace override if specified in the model configuration.
                default_models[model_config["name"]].trace = global_trace
                if "trace" in model_config:
                    default_models[model_config["name"]].trace = model_config["trace"]

            else:
                # Add new model if it doesn't exist in default models.
                default_model = load_model_info(
                    Path(model_config["source"]),
                    model_includes,
                )

                # Apply parameter overrides
                for k, p in default_model.parameters.items():
                    if k in global_parameters:
                        p.value = global_parameters[k].value
                    if k in model_config.get("parameters", {}):
                        p.value = model_config["parameters"][k]

                # Reload model
                default_model = load_model_info(
                    Path(model_config["source"]),
                    model_includes,
                    default_model.parameters,
                )
                default_model.name = model_config["name"]
                default_model.trace = global_trace
                if "trace" in model_config:
                    default_model.trace = model_config["trace"]
                default_models[default_model.name] = default_model
        return cls(
            name=config.get("name", ""),
            sources=global_sources,
            include_dirs=global_include_dirs,
            parameters=global_parameters,
            trace=global_trace,
            include_dspsim_library=config.get("include_dspsim_library", False),
            models=default_models,
        )
