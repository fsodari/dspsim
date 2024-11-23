"""Test reading the pyproject.toml config"""

from importlib.metadata import metadata
from pathlib import Path
import tomllib

from dspsim.config import Config


def test_metadata():
    # pyproject_path = Path("pyproject.toml")
    config = Config.from_pyproject()
    print(config)
    # print(dspsim_config.keys())
    # print(pyproject.keys())
