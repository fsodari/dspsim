from dspsim.verilator import verilate_json
from pathlib import Path
from dataclasses import dataclass, field
from pydantic import BaseModel, BeforeValidator
from typing import Annotated, Any

# @dataclass
# class TypeEntry:
#     type: str
#     name: str
#     addr: str
#     loc: str
#     dtypep: str
#     # Optional
#     keyword: str | None = None
#     range: str | None = None
#     generic: bool | None = None
#     signed: bool | None = None
#     rangep: list[str] | None = None

#     # Multi-dimensional fields.
#     declRange: str | None = None
#     refDTypep: str | None = None
#     childDTypep: str | None = None


# @dataclass
class VOIDDTYPE(BaseModel):
    type: str
    name: str
    addr: str
    # loc: str

    @classmethod
    def from_dict(cls, data: dict) -> VOIDDTYPE:
        return cls(**data)


def parse_const(const_str: str) -> int:
    """Parse a constant string and return it as an int or float."""
    hex_part = const_str.split("h")[-1]
    return int(hex_part, 16)


# @dataclass
class CONST(VOIDDTYPE):
    # dtypep: str
    name: Annotated[int, BeforeValidator(parse_const)]


def parse_p[T](list_input: list[T]) -> T | None:
    return list_input[0] if list_input else None


def parse_range(range_str: str) -> tuple[int, int]:
    """Parse a range string and return a tuple of two integers."""
    parts = range_str.strip("[]").split(":")
    return int(parts[0]), int(parts[1])


# @dataclass
class RANGE(VOIDDTYPE):
    ascending: bool
    fromBracket: bool
    leftp: Annotated[CONST, BeforeValidator(parse_p)]
    rightp: Annotated[CONST, BeforeValidator(parse_p)]

    @classmethod
    def from_dict(cls, data: dict) -> RANGE:
        return cls(**data)


# def parse_rangep(range_list: list[RANGE]) -> RANGE | None:
#     """Parse a list of RANGE objects and return a single RANGE object."""
#     # Implement your parsing logic here
#     # For now, just return the first RANGE in the list if it exists
#     return range_list[0] if range_list else None


# @dataclass
class BASICDTYPE(VOIDDTYPE):
    # dtypep: str
    keyword: str
    # generic: bool
    # rangep: RANGE | None = None
    # range: tuple[int, int] | None = None
    range: Annotated[tuple[int, int], BeforeValidator(parse_range)] | None = None
    signed: bool = False

    @classmethod
    def from_dict(cls, data: dict) -> BASICDTYPE:
        _rangep = data.get("rangep", [])
        _rangep_entry = None if len(_rangep) == 0 else _rangep[0]
        data["rangep"] = _rangep_entry
        return cls(**data)


# @dataclass
class UNPACKARRAYDTYPE(VOIDDTYPE):
    # dtypep: str
    declRange: Annotated[tuple[int, int], BeforeValidator(parse_range)]
    refDTypep: str
    # childDTypep: str
    rangep: RANGE
    signed: bool = False

    @classmethod
    def from_dict(cls, data: dict) -> UNPACKARRAYDTYPE:
        # _childDTypep = data.get("childDTypep", [])
        # _childDTypep_entry = None if len(_childDTypep) == 0 else _childDTypep[0]
        # data["childDTypep"] = _childDTypep_entry

        _rangep = data.get("rangep", [])
        _rangep_entry = None if len(_rangep) == 0 else _rangep[0]
        data["rangep"] = _rangep_entry
        return cls(**data)


type_map: dict[str, type[VOIDDTYPE]] = {
    "VOIDDTYPE": VOIDDTYPE,
    "BASICDTYPE": BASICDTYPE,
    "CONST": CONST,
    "RANGE": RANGE,
    "UNPACKARRAYDTYPE": UNPACKARRAYDTYPE,
}


def build_type_table(json_output) -> dict[str, VOIDDTYPE]:
    type_table_json = json_output["miscsp"][0]["typesp"]
    type_table = {t["addr"]: type_map[t["type"]].from_dict(t) for t in type_table_json}
    return type_table


def test_verilate_json():
    # Example test for verilate_json function
    hdl_file = Path("src/dspsim/hdl/HellModel.sv")
    json_output = verilate_json(
        [hdl_file],
    )
    type_table = build_type_table(json_output)
    assert type_table is not None
    for t in type_table.values():
        print()
        print(t)
