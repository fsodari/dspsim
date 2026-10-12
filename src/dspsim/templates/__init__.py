"""The Jinja templates of the generated code."""

from pathlib import Path

import jinja2

_template_env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(Path(__file__).parent)
)


def cmake_quote(value: str) -> str:
    """A string as one quoted CMake argument: nothing in it is expanded or splits it into a list."""
    escaped = "".join("\\" + c if c in '\\"$;' else c for c in str(value))
    return f'"{escaped}"'


_template_env.filters["cmake_quote"] = cmake_quote


def render_template(template_name: str, **kwargs):
    template = _template_env.get_template(template_name)
    return template.render(**kwargs)
