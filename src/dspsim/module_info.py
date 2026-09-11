"""Module information."""

from dataclasses import dataclass
from pathlib import Path

type ParamValueT = int | float | str | list["ParamValueT"]

_cnv_type_table: dict[str, type[int | str | float]] = {
    "bit": int,
    "logic": int,
    "int": int,
    "real": float,
    "string": str,
}


@dataclass
class DType:
    keyword: str
    width: int | None
    signed: bool
    shape: tuple[int, ...]

    @property
    def cnv_type(self) -> type[int | str | float]:
        return _cnv_type_table[self.keyword]


@dataclass
class Parameter:
    name: str
    dtype: DType
    value: ParamValueT


@dataclass
class Port:
    name: str
    dtype: DType
    direction: str


@dataclass
class ModuleInfo:
    name: str
    source: Path
    # source_filename: Path
    # source_realpath: Path

    parameters: dict[str, Parameter]
    ports: dict[str, Port]
