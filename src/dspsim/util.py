"""Miscellaneous Utilities"""

from pathlib import Path


def cmake_dir() -> Path:
    return Path(__file__).parent / "cmake"


def include_dir() -> Path:
    return Path(__file__).parent / "include"


def hdl_dir() -> Path:
    return Path(__file__).parent / "hdl"
