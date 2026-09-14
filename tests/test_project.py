"""Test Project config"""

from pathlib import Path

from dspsim.framework.project import ProjectConfig


def test_read_project_config():
    config = ProjectConfig.from_toml(Path("pyproject.toml"))
    print()
    print(config.report())
