"""Test reading the pyproject.toml config"""

from importlib.metadata import metadata
from pathlib import Path
import tomllib

from dspsim.config import Config

from dspsim.generate import Parameter, read_parameters, Port, read_ports
from dspsim.generate import verilator
import json
from tempfile import TemporaryDirectory

import pytest


# @pytest.mark.skip(reason="Testing other things.")
def test_read_parameters():
    source = Path("src/dspsim/hdl/SomeModel.sv")

    with TemporaryDirectory() as _tmpdir:
        # tmpdir = Path(_tmpdir)
        tmpdir = Path("local")

        verilator([source, "--json-only", "--Mdir", tmpdir])
        tree_file_path = tmpdir / f"V{source.stem}.tree.json"
        with open(tree_file_path) as fp:
            json_tree = json.load(fp)

        parameters = read_parameters(json_tree)
        # print(parameters)
        for name, param in parameters.items():
            print(f"{name}: {param}")

        ports = read_ports(json_tree)
        for name, port in ports.items():
            print(f"{name}: {port}")


# @pytest.mark.skip(reason="Testing other things.")
def test_read_config():
    pyproject_path = Path("pyproject.toml")
    config = Config.from_pyproject(pyproject_path)
    # print(config)
    print(config.to_json(indent=2))
