"""Build PSoC Projects."""

from dataclasses import dataclass
from dataclass_wizard import YAMLWizard
from pathlib import Path
import shutil
import subprocess
import os
import argparse


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


@dataclass
class Args:
    project: list[str]
    clean: bool
    build: bool
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
            "-config",
            type=Path,
            default=Path("psoc_config.yaml"),
            help="PSoC Creator Configuration file.",
        )

        return cls(**vars(parser.parse_args(cli_args)))


def main(cli_args: list[str] = None):
    args = Args.parse_args(cli_args)
    psoc_creator = PSoCCreatorConfig.from_yaml_file(args.config)
    # print(args)
    # print(psoc_creator)

    for project in psoc_creator.projects:
        if args.clean:
            psoc_creator.clean(project)
        if args.build:
            psoc_creator.build(project)


if __name__ == "__main__":
    main()
