"""Test Project config"""

from pathlib import Path
from tempfile import TemporaryDirectory

from dspsim.framework.project import ProjectConfig

hdl_dir = Path(__file__).parent / "test_modules"

example_pyproject = f"""
# tool configuration example.
[tool.dspsim]
name = "library"
# Use all sources in the dspsim/hdl directory.
sources = ["{hdl_dir.absolute().as_posix()}/*.sv", "{hdl_dir.absolute().as_posix()}/*.v"]
exclude_sources = ["{hdl_dir.absolute().as_posix()}/HellModel.sv"]
include_dirs = ["{hdl_dir.absolute().as_posix()}"]
# Can use fst on linux.
trace = "vcd"
# parameters = {{ DW = 24, CFGAW = 32, CFGDW = 32, COEFW = 18, COEFQ = 16 }}

# Global parameters applied to all modules.
[tool.dspsim.parameters]
DW = 24
CFGAW = 32
CFGDW = 32
COEFW = 18
COEFQ = 16

[tool.dspsim.models]
SimpleModel = {{source = "SimpleModel.sv", parameters = {{ DW = 32 }} }}
SimpleModel16 = {{source = "SimpleModel.sv", parameters = {{ DW = 16 }} }}

[tool.dspsim.models.SimpleModel8]
source = "SimpleModel.sv"
parameters = {{ DW = 8 }}
trace = "None"

#---------------------------------------------------------------------------------------------------#
#---------------------------------------------------------------------------------------------------#
"""


def test_read_project_config():
    with TemporaryDirectory() as temp_dir:
        pyproject_path = Path(temp_dir) / "pyproject.toml"
        pyproject_path.write_text(example_pyproject)
        config = ProjectConfig.from_toml(pyproject_path)
        print()
        print(config.report())
