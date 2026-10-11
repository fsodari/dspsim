import importlib.metadata
from pathlib import Path

__version__ = importlib.metadata.version("dspsim")


def cmake_dir() -> Path:
    """Return the path to the CMake directory."""
    return Path(__file__).parent / "cmake"


def include_dir() -> Path:
    """Return the path to the include directory."""
    return Path(__file__).parent / "include"


def version() -> str:
    return __version__


def link_module(module):
    import atexit

    from dspsim._framework import _get_global_context_factory

    module._set_global_context_factory(_get_global_context_factory())

    atexit.register(module._reset_global_context_factory)
