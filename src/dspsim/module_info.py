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
    width: int
    value: ParamValueT

    @property
    def ctype(self) -> str:
        if self.keyword in ["bit", "int", "logic"]:
            return "int"
        elif self.keyword in ["real"]:
            return "double"
        elif self.keyword in ["string"]:
            # return "const char*"
            return "std::string"
        else:
            raise ValueError(f"Unsupported keyword: {self.keyword}")

    @property
    def gparam_val(self) -> str:
        if self.keyword in ["string"]:
            return f'"{self.value}"'
        elif self.keyword in ["logic"]:
            if self.width == 32 or self.width == 64:
                return str(self.value)
            elif self.width % 4 == 0:
                return f"{self.width}'h{self.value:x}"
            else:
                # return binary representation
                return f"{self.width}'b{self.value:b}"
        else:
            return str(self.value)

    @property
    def cpp_val(self) -> str:
        if self.keyword in ["string"]:
            return f'"{self.value}"'
        else:
            return str(self.value)


@dataclass
class Port:
    name: str
    keyword: str
    signed: bool
    width: int
    direction: str
    shape: tuple[int, ...]

    @property
    def stdint_size(self) -> int:
        if self.width <= 8:
            return 8
        elif self.width <= 16:
            return 16
        elif self.width <= 32:
            return 32
        elif self.width <= 64:
            return 64
        else:
            raise ValueError(f"Unsupported width: {self.width}")


@dataclass
class ModuleInfo:
    name: str
    source: Path
    parameters: dict[str, Parameter]
    ports: dict[str, Port]
    trace: Literal["vcd", "fst"] | None = None
