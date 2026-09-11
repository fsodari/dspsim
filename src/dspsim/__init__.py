import importlib.metadata
from pathlib import Path

__version__ = importlib.metadata.version(str(__package__))


def hdl_dir() -> Path:
    """Return the path to the HDL directory."""
    return Path(__file__).parent / "hdl"


def include_dir() -> Path:
    """Return the path to the include directory."""
    return Path(__file__).parent / "include"
