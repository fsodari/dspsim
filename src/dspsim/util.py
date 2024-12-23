"""Miscellaneous Utilities"""

from pathlib import Path
import functools as _functools
import numpy as np


def cmake_dir() -> Path:
    return Path(__file__).parent / "lib/cmake/dspsim"


def include_dir() -> Path:
    return Path(__file__).parent / "include"


def hdl_dir() -> Path:
    return Path(Path(__file__).parent / "hdl").absolute()


def lib_dir() -> Path:
    return Path(__file__).parent / "lib"


import jinja2

_template_env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(Path(__file__).parent / "templates")
)


def render_template(template: str, **kwargs) -> str:
    """"""
    return _template_env.get_template(template).render(kwargs)


@_functools.cache
def uint_width(width: int) -> int:
    if width <= 8:
        return 8
    elif width <= 16:
        return 16
    elif width <= 32:
        return 32
    elif width <= 64:
        return 64


def to_fixed(flt: float, q: int) -> int:
    """"""
    return flt * (2**q)


def to_float(fxd: int, q: int) -> float:
    """"""
    return fxd / (2**q)


def sign_extend(value: int, width: int) -> int:
    sign_bit = 1 << (width - 1)
    return (value & (sign_bit - 1)) - (value & sign_bit)


def sign_extendv(data: np.ndarray, width: int) -> int:
    sign_bit = 1 << (width - 1)
    mask0 = sign_bit - 1

    vxtnd = np.vectorize(lambda x: (x & mask0) - (x & sign_bit))

    return vxtnd(data)
