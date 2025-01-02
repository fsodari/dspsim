"""Build PSoC Projects."""

from dataclasses import dataclass
from dataclass_wizard import YAMLWizard
from pathlib import Path
import shutil
import subprocess
import os
import argparse

from dspsim import avril
from dspsim.avril import Avril, AvrilAck, find_device, DType, ErrorCode
import time

BOOTLOADER_SECURITY_KEY = "0x424344454647"


@dataclass
class PSoCCreatorConfig(YAMLWizard):
    workspace: Path
    projects: dict[str, Path]
    build_type: str = "Release"

    psoc_creator_location: Path = Path("C:/Program Files (x86)/Cypress/PSoC Creator")
    psoc_creator_version: str = "4.4"
    arch: str = "CortexM3"
    compiler: str = "ARM_GCC_541"

    @property
    def cyprjmgr(self) -> Path:
        """Path to cyprjmgr utility"""
        return (
            self.psoc_creator_location
            / self.psoc_creator_version
            / "PSoC Creator/bin/cyprjmgr.exe"
        )

    def build_dir(self, project: str) -> Path:
        return self.projects[project] / self.arch / self.compiler / self.build_type

    def generated_source_dir(self, project: str) -> Path:
        return self.projects[project] / "Generated_Source"

    def cyacd_file(self, project: str) -> Path:
        return self.build_dir(project) / f"{project}.cyacd"

    def clean(self, project: str):
        """Deletes all generated sources, cyfit file, and runs Clean in PSoC Creator for all projects in the workspace."""
        prj_root = self.projects[project]

        # Delete all of generated source and cyfit file.
        print(f"Deleting {self.generated_source_dir(project)}...")
        shutil.rmtree(self.generated_source_dir(project), ignore_errors=True)

        print(f"Deleting {self.build_dir(project)}...")
        shutil.rmtree(self.build_dir(project), ignore_errors=True)
        cyfit = prj_root / f"{prj_root.stem}.cyfit"
        if cyfit.exists():
            print(f"Deleting {cyfit}...")
            os.remove(cyfit)

        # Run the psoc clean command.
        print(f"Cleaning {project}...")
        cmd = [
            self.cyprjmgr,
            "-wrk",
            self.workspace,
            "-clean",
            "-prj",
            project,
        ]
        subprocess.run(cmd, check=True)

    def build(self, project: str):
        """Build project."""
        print(f"Building {project}...")
        cmd = [
            self.cyprjmgr,
            "-wrk",
            self.workspace,
            "-build",
            "-prj",
            project,
            "-c",
            self.build_type,
        ]
        subprocess.run(cmd, check=True)

    def bootload(self, project: str, recovery: bool = False, password: int = 0):
        """"""
        print(f"Bootloading {project}")
        if not recovery:
            with Avril(avril.AvrilMode.Bootload) as av:
                ack: AvrilAck = av.write_reg(0, password, dtype=DType.int32)
                if ack.error != ErrorCode.NoError:
                    raise Exception(f"Enter Bootload Failed: {ack.error.name}")
            time.sleep(0.5)

        bootload_port = find_device(avril.BOOTLOAD_PID)[0]
        cyflash_cmd = [
            "cyflash",
            f"--serial={bootload_port}",
            self.cyacd_file(project),
            "--timeout=1.0",
            "--psoc5",
            "-cs=48",
            f"--key={BOOTLOADER_SECURITY_KEY}",
        ]
        subprocess.run(cyflash_cmd, check=True)


@dataclass
class Args:
    project: list[str]
    clean: bool
    build: bool
    bootload: bool
    recovery: bool
    password: int
    config: Path = Path("psoc_config.yaml")

    @classmethod
    def parse_args(cls, cli_args: list[str] = None):
        parser = argparse.ArgumentParser()
        parser.add_argument(
            "project", nargs="+", type=str, help="Project(s) to interact with."
        )
        parser.add_argument("--clean", action="store_true", help="Clean project(s).")
        parser.add_argument("--build", action="store_true", help="Build project(s).")
        parser.add_argument(
            "--bootload", action="store_true", help="Run the bootload host tool."
        )
        parser.add_argument("--recovery", action="store_true")
        parser.add_argument("-password", type=int, default=0, help="Bootload password")
        parser.add_argument(
            "-config",
            type=Path,
            default=Path("psoc_config.yaml"),
            help="PSoC Creator Configuration file.",
        )

        return cls(**vars(parser.parse_args(cli_args)))


def main(cli_args: list[str] = None):
    args = Args.parse_args(cli_args)
    psoc_creator = PSoCCreatorConfig.from_yaml_file(args.config)

    for project in args.project:
        if args.clean:
            psoc_creator.clean(project)
        if args.build:
            psoc_creator.build(project)
        if args.bootload:
            psoc_creator.bootload(project, args.recovery, args.password)


if __name__ == "__main__":
    main()
