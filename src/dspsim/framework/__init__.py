# import importlib.metadata
from pathlib import Path

# __version__ = importlib.metadata.version(str(__package__))
# __version__ = "0.4.0"


def include_dir() -> Path:
    """Return the path to the include directory."""
    return Path(__file__).parent / "include"


def cmake_dir() -> Path:
    """Return the path to the CMake directory."""
    return Path(__file__).parent / "cmake"


def version() -> str:
    import importlib.metadata

    return importlib.metadata.version(str(__package__))


import atexit
import threading
from contextlib import contextmanager

from dspsim.framework._framework import (
    Clock,
    Dff8,
    Dff16,
    Dff32,
    Dff64,
    ModuleName,
    Signal8,
    Signal16,
    Signal32,
    Signal64,
    get_global_context_factory,
    reset_global_context_factory,
    # set_global_context_factory,
)
from dspsim.framework._framework import Context as _Context
from dspsim.framework._framework import Model as _Model
from dspsim.framework._framework import Module as _Module

# Prevent nb leak warnings.
atexit.register(reset_global_context_factory)


class Context(_Context):
    """
    Python wrapper for the C++ Context class.
    In multi-threaded applications, only one thread can
    instantiate models at a time. Threads must call release() when finished instatiating models,
    then they can proceed to simulation/elaboration.
    """

    locked: bool = False
    _global_context_lock: threading.Lock = threading.Lock()

    @classmethod
    def obtain_lock(cls, name: str = ""):
        Context._global_context_lock.acquire()
        inst = cls(name)
        inst.locked = True
        return inst

    def __del__(self):
        self.release()

        # Calling clear here prevents nanobind leak warnings.
        self.clear()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.clear()
        self.release()

    def release(self):
        self.reset()
        if self.locked and Context._global_context_lock.locked():
            self.locked = False
            Context._global_context_lock.release()

    @contextmanager
    def construct(self):
        """
        Release the global context once all models have been instantiated in the context.
        """
        try:
            yield
        finally:
            # Elaborate at end of construction
            self.elaborate()
            # Release the global context lock after elaboration
            self.release()


class Model(_Model):
    """
    Use Python models in a simulation. Subclasses of this class MUST call super().__init__() in their constructor.
    """

    def __init__(self, kind: str = "model"):
        super().__init__(kind)
        # Register the model with its context.
        self.context.own_model(self)

    def __repr__(self):
        return f"<{self.__class__.__name__} id={self.id} context_id={self.context.id}>"

    def __str__(self):
        return self.__repr__()


class Module(_Module):
    """
    Python wrapper for the C++ Module class.
    """

    def __init__(self, name: str):
        super().__init__(ModuleName(name))


def signal(name: str, init: int = 0, width: int = 32, is_signed: bool = False):
    if width <= 8:
        return Signal8(name, width, init, is_signed)
    elif width <= 16:
        return Signal16(name, width, init, is_signed)
    elif width <= 32:
        return Signal32(name, width, init, is_signed)
    elif width <= 64:
        return Signal64(name, width, init, is_signed)
    else:
        raise ValueError("Unsupported signal width")


__all__ = [
    "Clock",
    "Context",
    "Dff8",
    "Dff16",
    "Dff32",
    "Dff64",
    "Model",
    "Signal8",
    "Signal16",
    "Signal32",
    "Signal64",
    "get_global_context_factory",
    # "set_global_context_factory",
    "signal",
]
