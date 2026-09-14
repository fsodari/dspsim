import argparse
from dataclasses import dataclass
from pathlib import Path

import jinja2

from dspsim.framework.project import ProjectConfig


@dataclass
class Args:
    pyproject: Path
    output_dir: Path
    verbose: bool

    @classmethod
    def parse_args(cls, args=None):
        parser = argparse.ArgumentParser(description="Run dspsim.generate script")
        parser.add_argument("--pyproject", required=True, help="Path to pyproject.toml")
        parser.add_argument(
            "--output-dir", required=True, help="Output directory for generated files"
        )
        parser.add_argument(
            "-v", "--verbose", action="store_true", help="Enable verbose output"
        )
        parsed_args = parser.parse_args(args)
        return cls(
            pyproject=Path(parsed_args.pyproject),
            output_dir=Path(parsed_args.output_dir),
            verbose=parsed_args.verbose,
        )


_template_env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(Path(__file__).parent / "templates")
)


def render_template(template_name: str, **kwargs):
    template = _template_env.get_template(template_name)
    return template.render(**kwargs)


def main():
    args = Args.parse_args()

    def _print(*_args, **_kwargs):
        if args.verbose:
            print(*_args, **_kwargs)

    project_config = ProjectConfig.from_toml(args.pyproject)
    _print(f"Loaded project configuration:\n{project_config.report()}")

    if not args.output_dir.exists():
        args.output_dir.mkdir(parents=True)

    # Generate the library module source code using the template.
    library_module = render_template("library_module.cpp.jinja", config=project_config)
    _print(f"Generated library module:\n{library_module}")
    with open(args.output_dir / f"{project_config.name}.cpp", "w") as f:
        f.write(library_module)

    # Generate verilate CMake include file.
    verilate_include = render_template(
        "verilate_include.cmake.jinja", config=project_config
    )
    _print(f"Generated verilate include:\n{verilate_include}")
    with open(args.output_dir / f"{project_config.name}_include.cmake", "w") as f:
        f.write(verilate_include)

    # Generate the models
    for model in project_config.models.values():
        model_code = render_template("model.h.jinja", model=model)
        _print(f"Generated model {model.name}:\n{model_code}")
        with open(args.output_dir / f"{model.name}.h", "w") as f:
            f.write(model_code)

        # # verilator args template
        # vargs = render_template("verilator_args.txt.jinja", model=model)
        # _print(f"Generated Verilator args for {model.name}:\n{vargs}")
        # with open(args.output_dir / f"{model.name}_verilator_args.txt", "w") as f:
        #     f.write(vargs)
