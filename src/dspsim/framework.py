"""
Framework module for dspsim.
This module provides the Context and Model classes for building and simulating models.
"""

import threading
from contextlib import contextmanager

from dspsim._framework import Clock, Signal8, Signal16, Signal32, Signal64, Simulator
from dspsim._framework import Context as _Context
from dspsim._framework import Model as _Model

global_context_lock = threading.Lock()


class Context(_Context):
    """
    Python wrapper for the C++ Context class.
    In multi-threaded applications, only one thread can
    instantiate models at a time. Threads must call release() when finished instatiating models,
    then they can proceed to simulation/elaboration.
    """

    locked: bool = False

    def __new__(cls):
        global_context_lock.acquire()
        inst = super().__new__(cls)
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
        if self.locked and global_context_lock.locked():
            self.reset_global_context()
            self.locked = False
            global_context_lock.release()

    @contextmanager
    def construct(self):
        """
        Release the global context once all models have been instantiated in the context.
        """
        try:
            yield
        finally:
            self.release()


class Model(_Model):
    """
    Use Python models in a simulation. Subclasses of this class MUST call super().__init__() in their constructor.
    """

    def __init__(self, kind: str = "model"):
        super().__init__(kind)
        # Register the model with its context.
        self.context.register_model(self)

    def __repr__(self):
        return f"<{self.__class__.__name__} id={self.id} context_id={self.context.id}>"

    def __str__(self):
        return self.__repr__()


def signal(init: int = 0, width: int = 32, is_signed: bool = False):
    if width <= 8:
        return Signal8(init, width, is_signed)
    elif width <= 16:
        return Signal16(init, width, is_signed)
    elif width <= 32:
        return Signal32(init, width, is_signed)
    elif width <= 64:
        return Signal64(init, width, is_signed)
    else:
        raise ValueError("Unsupported signal width")


__all__ = [
    "Clock",
    "Context",
    "Model",
    "Signal8",
    "Signal16",
    "Signal32",
    "Signal64",
    "Simulator",
    "signal",
]
