"""The Jinja templates of the generated code."""

from pathlib import Path

import jinja2

_template_env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(Path(__file__).parent)
)


def render_template(template_name: str, **kwargs):
    template = _template_env.get_template(template_name)
    return template.render(**kwargs)
