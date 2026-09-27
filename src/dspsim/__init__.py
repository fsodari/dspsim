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

    from dspsim.framework import get_global_context_factory

    module.set_global_context_factory(get_global_context_factory())

    atexit.register(module.reset_global_context_factory)
