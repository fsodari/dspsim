"""Tools for building a vivado project"""

import subprocess
import shutil
import os
import sys
from string import Template
from pathlib import Path
from dataclasses import dataclass
from dataclass_wizard import YAMLWizard
from contextlib import contextmanager


# Vivado Project Template
viv_prj_tcl = Template(
    """# Auto-generated code.
create_project ${name} -force
set_property part ${fpga_part} [current_project]
# Source Files
${source_list}

# Constraints
${constraint_sets}
${constraints}
# Project config
set_property include_dirs [list .] [get_filesets sources_1]
set_property top ${top_module} [current_fileset]
set_property source_mgmt_mode All [current_project]
set_property constrset ${active_constraint_set} [get_runs synth_1]
set_property constrset ${active_constraint_set} [get_runs impl_1]
set_property SCOPED_TO_REF TopDesign [get_files -all -of_objects [get_fileset sources_1] {bootloader.elf}]
set_property SCOPED_TO_CELLS { microblaze_0 } [get_files -all -of_objects [get_fileset sources_1] {bootloader.elf}]
"""
)

flash_prj_tcl = Template(
    """# Auto-generated code.
create_project ${name} -force
"""
)

# Build Script.
viv_build_tcl = Template(
    """reset_run synth_1
set_property STEPS.WRITE_BITSTREAM.ARGS.BIN_FILE true [get_runs impl_1]
# Specify constraint set
set_property constrset ${active_constraint_set} [get_runs synth_1]
set_property constrset ${active_constraint_set} [get_runs impl_1]
launch_runs impl_1 -to_step write_bitstream
wait_on_run impl_1
if { [get_property PROGRESS [get_runs impl_1]] != "100%"} {
   puts "ERROR: Implementation and bitstream generation step failed."
   exit 1
} else {
    puts "Bitstream generation completed"
}
"""
)

#
program_tcl = Template(
    """open_hw
connect_hw_server 
open_hw_target
current_hw_device [get_hw_devices ${fpga_family}_0]
refresh_hw_device -update_hw_probes false [lindex [get_hw_devices ${fpga_family}_0] 0]
set_property PROBES.FILE {} [get_hw_devices ${fpga_family}_0]
set_property FULL_PROBES.FILE {} [get_hw_devices ${fpga_family}_0]
set_property PROGRAM.FILE {${bitstream_file}} [get_hw_devices ${fpga_family}_0]
program_hw_devices [get_hw_devices ${fpga_family}_0]
refresh_hw_device [lindex [get_hw_devices ${fpga_family}_0] 0]
"""
)

# TCL Script Template.
program_flash_tcl = Template(
    """# Vivado TCL Script to program flash.
# Open vivado hw manager
open_hw
connect_hw_server 
open_hw_target
current_hw_device [get_hw_devices ${fpga_family}_0]
refresh_hw_device -update_hw_probes false [lindex [get_hw_devices ${fpga_family}_0] 0]
# Set up device
create_hw_cfgmem -hw_device [lindex [get_hw_devices ${fpga_family}_0] 0] [lindex [get_cfgmem_parts {${flash_part}}] 0]
set_property PROGRAM.BLANK_CHECK  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.ERASE  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.CFG_PROGRAM  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.VERIFY  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.CHECKSUM  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
refresh_hw_device [lindex [get_hw_devices ${fpga_family}_0] 0]
# Program
set_property PROGRAM.ADDRESS_RANGE  {use_file} [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.FILES [list "${flash_file}" ] [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.PRM_FILE {} [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.UNUSED_PIN_TERMINATION {pull-none} [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.BLANK_CHECK  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.ERASE  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.CFG_PROGRAM  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.VERIFY  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
set_property PROGRAM.CHECKSUM  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
startgroup 
create_hw_bitstream -hw_device [lindex [get_hw_devices ${fpga_family}_0] 0] [get_property PROGRAM.HW_CFGMEM_BITFILE [ lindex [get_hw_devices ${fpga_family}_0] 0]]; program_hw_devices [lindex [get_hw_devices ${fpga_family}_0] 0]; refresh_hw_device [lindex [get_hw_devices ${fpga_family}_0] 0];
program_hw_cfgmem -hw_cfgmem [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices ${fpga_family}_0] 0]]
endgroup
"""
)

src_file_options = {
    ".v": "read_verilog ",
    ".sv": "read_verilog -sv ",
    ".vhd": "read_vhdl ",
    ".mem": "read_mem ",
    ".tcl": "source ",
    ".xci": "read_ip ",
    ".xcix": "read_ip ",
    ".xdc": "read_xdc ",
    ".elf": "add_files -norecurse ",
}


@contextmanager
def enter_dir(dir: Path):
    original = Path(os.getcwd()).absolute()
    try:
        os.makedirs(dir, exist_ok=True)
        os.chdir(dir)
        yield
    finally:
        os.chdir(original)


def src_entries(files: list[Path]) -> list[str]:
    """Convert a path to its formatted tcl command."""
    # File entry command is based on file type.
    entries = []
    for f in files:
        if f.suffix in src_file_options:
            entries.append(src_file_options[f.suffix] + f.absolute().as_posix())
    return entries


def constraint_entry(constraint_file: Path, constraint_set: str) -> str:
    """"""
    return f"add_files -fileset {constraint_set} -norecurse {constraint_file.absolute().as_posix()}"


@dataclass
class VivadoConfig(YAMLWizard):
    # project_name: str
    name: str
    version: str
    top_module: str
    update_file: Path
    synthesis_source_dirs: list[str]
    constraints: dict[str, list[str]]
    project_dir: Path
    outputs: list[str]
    vivado: str
    fpga_part: str
    flash_part: str
    # src_mgmt_mode: str
    # ip: list[str] = field(default_factory=list)

    def _set_os(self):
        """"""
        self.update_file = (
            Path(self.project_dir) / f"{self.name}.runs" / "impl_1" / self.update_file
        )
        self.outputs = [
            str(Path(self.project_dir) / f"{self.name}.runs" / "impl_1" / Path(o).name)
            for o in self.outputs
        ]

    def __post_init__(self):
        """If the version is given in a file, parse it."""
        # version_file = Path(self.version)

        # if version_file.exists():
        #     self.version = version_from_file(version_file)

        self._set_os()
        if Path(self.vivado).parent.stem == "Vivado":
            # print("Using Vivado")
            self._use_lab = False
        else:
            # print("Using Vivado Lab")
            self._use_lab = True

    def create(self, flash_only: bool = False):
        """"""
        os.makedirs(self.project_dir, exist_ok=True)

        print("Generating TCL Files...")
        self.gen_prj_tcl(flash_only=flash_only)

        if not flash_only:
            self.gen_build_tcl()
            self.gen_program_tcl()
        self.gen_program_flash_tcl()

        v = "vivado_lab" if self._use_lab else "vivado"
        gen_prj_cmd = f"{v} -notrace -mode batch -source {self.name}.tcl"

        print("Creating Vivado Project...")
        self.vivado_command(gen_prj_cmd)

    def clean(self):
        """"""
        shutil.rmtree(self.project_dir, ignore_errors=True)

    def build(self):
        """"""
        print("Regenerating build .tcl script.")
        self.gen_build_tcl()

        print("Building Vivado Project...")
        build_cmd = (
            f"vivado -notrace -mode batch -source {self.name}_build.tcl {self.name}.xpr"
        )
        self.vivado_command(build_cmd)

    def install(self, output_dir: Path = Path()):
        """Install output files into a particular directory."""
        osub_dir = output_dir / f"{self.name}_{self.version}"
        os.makedirs(osub_dir, exist_ok=True)

        for f in self.outputs:
            shutil.copy(f, osub_dir / Path(f).name)

    def program(self):
        """"""
        print("Regenerating program.tcl script.")
        self.gen_program_tcl()

        print("Programming FPGA...")
        program_cmd = f"vivado -notrace -mode batch -source {self.name}_program.tcl {self.name}.xpr"
        self.vivado_command(program_cmd)

    def program_flash(self):
        """"""
        print("Regenerating program_flash.tcl script.")
        self.gen_program_flash_tcl()

        print("Programming FPGA Flash...")
        v = "vivado_lab" if self._use_lab else "vivado"

        # if self._use_lab:
        #     program_cmd = (
        #         f"{v} -notrace -mode batch -source {self.name}_program_flash.tcl"
        #     )
        # else:
        #     program_cmd = f"{v} -notrace -mode batch -source {self.name}_program_flash.tcl {self.name}.xpr"
        program_cmd = f"{v} -notrace -mode batch -source {self.name}_program_flash.tcl"
        self.vivado_command(program_cmd)

    def vivado_command(self, cmd: str) -> None:
        """Vivado command for Windows"""

        if sys.platform == "win32":
            vivado_settings = Path(self.vivado) / "settings64.bat"
            shell_cmd = ["cmd.exe", "/c"]
            vivado_prefix = vivado_settings.as_posix()
            vivado_cmd = shell_cmd + [" && ".join([vivado_prefix, cmd])]
        else:
            vivado_settings = Path(self.vivado) / "settings64.sh"
            vivado_prefix = f"wsl.exe source {vivado_settings.as_posix()}"
            vivado_cmd = ";".join([vivado_prefix, cmd])

        with enter_dir(self.project_dir):
            subprocess.run(vivado_cmd, check=True)

    def gen_prj_tcl(self, flash_only: bool = False) -> None:
        """"""
        prj_tcl_file = Path(self.project_dir) / f"{self.name}.tcl"
        # Only create non-default sets.
        csets = [
            f"create_fileset -constrset {cset}"
            for cset in self.constraints
            if cset != "constrs_1"
        ]
        constrs = [
            constraint_entry(Path(cfile), cset)
            for cset in self.constraints
            for cfile in self.constraints[cset]
        ]

        source_entries = src_entries(
            [
                s
                for src_dir in self.synthesis_source_dirs
                for s in Path(src_dir).iterdir()
            ]
        )

        # ip = src_entries([Path(s) for s in self.ip])

        # Project TCL
        prj_tcl_filter = {
            "source_list": "\n".join(source_entries),
            # "ip_sources": "\n".join(ip),
            "constraint_sets": "\n".join(csets),
            "constraints": "\n".join(constrs),
            "active_constraint_set": list(self.constraints.keys())[0],
            "name": self.name,
            "fpga_part": self.fpga_part,
            "top_module": self.top_module,
        }

        prj_template = flash_prj_tcl if flash_only else viv_prj_tcl
        result = prj_template.safe_substitute(prj_tcl_filter)
        with open(prj_tcl_file, "w") as f:
            f.write(result)

    def gen_build_tcl(self, constraint_set: str = "constrs_1") -> None:
        """"""
        build_tcl_file = Path(self.project_dir) / f"{self.name}_build.tcl"
        result = viv_build_tcl.safe_substitute(
            {"active_constraint_set": constraint_set}
        )
        with open(build_tcl_file, "w") as f:
            f.write(result)

    def gen_program_tcl(self) -> None:
        """"""
        # Program TCL
        program_tcl_file = Path(self.project_dir) / f"{self.name}_program.tcl"

        fpga_family = self.fpga_part.split("t")[0] + "t"

        bitstream_file = ""
        for f in self.outputs:
            if f.endswith(".bit"):
                bitstream_file = f

        template_filter = {
            "flash_part": self.flash_part,
            "fpga_family": fpga_family,
            "bitstream_file": bitstream_file,
        }

        result = program_tcl.safe_substitute(template_filter)
        with open(program_tcl_file, "w") as f:
            f.write(result)

    def gen_program_flash_tcl(self) -> None:
        """"""
        # Program TCL
        program_flash_tcl_file = (
            Path(self.project_dir) / f"{self.name}_program_flash.tcl"
        )

        fpga_family = self.fpga_part.split("t")[0] + "t"

        flash_file = ""
        for f in self.outputs:
            if f.endswith(".bin"):
                flash_file = f

        template_filter = {
            "flash_part": self.flash_part,
            "fpga_family": fpga_family,
            "flash_file": flash_file,
        }

        result = program_flash_tcl.safe_substitute(template_filter)
        with open(program_flash_tcl_file, "w") as f:
            f.write(result)
