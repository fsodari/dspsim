"""Test Project config"""

from pathlib import Path

import dspsim.project
from dspsim.project import ProjectConfig


def test_read_project_config():
    config = ProjectConfig.from_toml(Path("pyproject.toml"))

    print(f"Name: {config.name}")
    print(f"include_dspsim_library: {config.include_dspsim_library}")
    print(f"Sources: {config.sources}")
    print("Include Dirs:")
    for include_dir in config.include_dirs:
        print(f"\t{include_dir}")
    print("Parameters")
    for param_name, param in config.parameters.items():
        print(f"\t{param_name}: {param}")
    print(f"trace: {config.trace}")

    for model in config.models.values():
        print(f"Model: {model.name}")
        print(f"\tSource: {model.source}")
        print("\tParameters:")
        for param in model.parameters.values():
            print(f"\t\t{param.name}: {param}")
        print("\tPorts:")
        for port in model.ports.values():
            print(f"\t\t{port.name}: {port}")
        print(f"\tTrace: {model.trace}")
    # print(f"models: {config.models}")
