"""Module information."""

from dataclasses import dataclass
from pathlib import Path

import numpy.typing as npt

# from typing import


@dataclass
class Parameter:
    name: str
    value: str | int | float | npt.ArrayLike
    shape: tuple[int, ...]


@dataclass
class Port:
    name: str
    width: int
    direction: str
    signed: bool
    shape: tuple[int, ...]


@dataclass
class Module:
    name: str
    source: Path
    parameters: dict[str, Parameter]
    ports: dict[str, Port]
