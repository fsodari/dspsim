import importlib.metadata
from pathlib import Path

__version__ = importlib.metadata.version("dspsim")


def _install_dir() -> Path:
    """
    The directory that the compiled parts of the package are installed in: the extension module, the core
    library, the headers and the CMake files. An editable install keeps the Python sources somewhere else.
    """
    import dspsim._framework as framework

    return Path(framework.__file__).parent


def cmake_dir() -> Path:
    """Return the path to the CMake directory."""
    return _install_dir() / "cmake"


def include_dir() -> Path:
    """Return the path to the include directory."""
    return _install_dir() / "include"


def version() -> str:
    return __version__


def link_module(module):
    import atexit

    from dspsim._framework import _get_global_context_factory

    module._set_global_context_factory(_get_global_context_factory())

    atexit.register(module._reset_global_context_factory)
