import argparse
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Args:
    pyproject: Path
    output_dir: Path

    @classmethod
    def parse_args(cls, args=None):
        parser = argparse.ArgumentParser(description="Run dspsim.generate script")
        parser.add_argument("--pyproject", required=True, help="Path to pyproject.toml")
        parser.add_argument(
            "--output-dir", required=True, help="Output directory for generated files"
        )
        parsed_args = parser.parse_args(args)
        return cls(
            pyproject=Path(parsed_args.pyproject),
            output_dir=Path(parsed_args.output_dir),
        )


def main():
    args = Args.parse_args()

    print(
        f"Running dspsim.generate script with pyproject: {args.pyproject} and output dir: {args.output_dir}"
    )
