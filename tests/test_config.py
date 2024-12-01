"""Test reading the pyproject.toml config"""

from pathlib import Path
from dspsim.generate import Config
import pytest


def test_read_tool_config():
    pyproject_path = Path("pyproject.toml")
    config = Config.from_pyproject(pyproject_path)
