import importlib.metadata
from pathlib import Path

__version__ = importlib.metadata.version("dspsim")


def include_dir() -> Path:
    """Return the path to the include directory."""
    return Path(__file__).parent / "include"


def cmake_dir() -> Path:
    """Return the path to the CMake directory."""
    return Path(__file__).parent / "cmake"


def version() -> str:
    return __version__
