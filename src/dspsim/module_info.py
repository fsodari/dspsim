"""HDL Module information."""

from dataclasses import dataclass
from pathlib import Path
from typing import Literal

type ParamValueT = int | float | str


@dataclass
class Parameter:
    name: str
    keyword: str
    signed: bool
    value: ParamValueT


@dataclass
class Port:
    name: str
    keyword: str
    signed: bool
    width: int
    direction: str
    shape: tuple[int, ...]


@dataclass
class ModuleInfo:
    name: str
    source: Path
    parameters: dict[str, Parameter]
    ports: dict[str, Port]
    trace: Literal["vcd", "fst"] | None = None
