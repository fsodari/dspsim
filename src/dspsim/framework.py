import atexit
import functools
import inspect
import threading
from contextlib import contextmanager

from dspsim._framework import (
    Clock,
    ContextConstructionError,
    DffS8,
    DffS16,
    DffS32,
    DffS64,
    DffU8,
    DffU16,
    DffU32,
    DffU64,
    InputArrayFloat,
    InputArrayS8,
    InputArrayS16,
    InputArrayS32,
    InputArrayS64,
    InputArrayU8,
    InputArrayU16,
    InputArrayU32,
    InputArrayU64,
    InputArrayViewFloat,
    InputArrayViewS8,
    InputArrayViewS16,
    InputArrayViewS32,
    InputArrayViewS64,
    InputArrayViewU8,
    InputArrayViewU16,
    InputArrayViewU32,
    InputArrayViewU64,
    InputFloat,
    InputS8,
    InputS16,
    InputS32,
    InputS64,
    InputU8,
    InputU16,
    InputU32,
    InputU64,
    Model,
    ModuleName,
    OutputArrayFloat,
    OutputArrayS8,
    OutputArrayS16,
    OutputArrayS32,
    OutputArrayS64,
    OutputArrayU8,
    OutputArrayU16,
    OutputArrayU32,
    OutputArrayU64,
    OutputArrayViewFloat,
    OutputArrayViewS8,
    OutputArrayViewS16,
    OutputArrayViewS32,
    OutputArrayViewS64,
    OutputArrayViewU8,
    OutputArrayViewU16,
    OutputArrayViewU32,
    OutputArrayViewU64,
    OutputFloat,
    OutputS8,
    OutputS16,
    OutputS32,
    OutputS64,
    OutputU8,
    OutputU16,
    OutputU32,
    OutputU64,
    SignalArrayFloat,
    SignalArrayS8,
    SignalArrayS16,
    SignalArrayS32,
    SignalArrayS64,
    SignalArrayU8,
    SignalArrayU16,
    SignalArrayU32,
    SignalArrayU64,
    SignalArrayViewFloat,
    SignalArrayViewS8,
    SignalArrayViewS16,
    SignalArrayViewS32,
    SignalArrayViewS64,
    SignalArrayViewU8,
    SignalArrayViewU16,
    SignalArrayViewU32,
    SignalArrayViewU64,
    SignalFloat,
    SignalS8,
    SignalS16,
    SignalS32,
    SignalS64,
    SignalU8,
    SignalU16,
    SignalU32,
    SignalU64,
    Wait,
    WaitBase,
    WaitSensitivityEvent,
    WaitTimeEvent,
    get_global_context_factory,
    reset_global_context_factory,
    # set_global_context_factory,
)
from dspsim._framework import Context as _Context
from dspsim._framework import Module as _Module

# Prevent nb leak warnings.
atexit.register(reset_global_context_factory)


class Context(_Context):
    """
    Python wrapper for the C++ Context class.

    Only one context can be under construction at a time, because models register with the
    global active context. Creating a Context takes a global lock that is released when the
    context is elaborated (or released). After elaboration the design is locked, the context
    detaches from the global context, and it can simulate independently of new contexts.

    - Another thread creating a Context blocks until the current one is elaborated.
    - The same thread creating a second Context before elaborating the first would deadlock,
      so it raises ContextConstructionError instead.
    """

    _construction_lock: threading.Lock = threading.Lock()
    # Thread ident of the lock holder. Only the holder sets it to its own ident, so a thread
    # can read it without the lock to check whether it already holds the lock.
    _construction_owner: int | None = None
    # Name of the context holding the lock, for error messages.
    _construction_holder: str = ""

    def __new__(cls, name: str = ""):
        cls._acquire_construction_lock(name)
        try:
            # nanobind binds the constructor with nb::new_, so at runtime _Context.__new__(cls, name)
            # creates the C++ context. The generated stub only declares __init__, so type checkers
            # fall back to object.__new__(cls) and reject the name argument.
            inst = super().__new__(cls, name)  # pyright: ignore[reportCallIssue]
        except BaseException:
            cls._release_construction_lock()
            raise
        inst._holds_construction_lock = True
        Context._construction_holder = inst.name
        return inst

    @classmethod
    def _acquire_construction_lock(cls, name: str):
        if Context._construction_owner == threading.get_ident():
            raise ContextConstructionError(
                f"Cannot create context '{name}': context '{Context._construction_holder}' is still under "
                "construction in this thread. Call elaborate() or release() on it first."
            )
        Context._construction_lock.acquire()
        Context._construction_owner = threading.get_ident()

    @staticmethod
    def _release_construction_lock():
        Context._construction_owner = None
        Context._construction_lock.release()

    @classmethod
    def obtain_lock(cls, name: str = ""):
        """Deprecated: creating a Context takes the construction lock."""
        return cls(name)

    @property
    def locked(self) -> bool:
        """True while this context holds the construction lock."""
        return getattr(self, "_holds_construction_lock", False)

    def __del__(self):
        self.release()

        # Calling clear here prevents nanobind leak warnings.
        self.clear()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.clear()
        self.release()

    def elaborate(self):
        """Finalize the design, detach from the global context, and release the construction lock."""
        try:
            super().elaborate()
        finally:
            self.release()

    def release(self):
        """
        Detach from the global context (only if this context is the active one)
        and release the construction lock if this context holds it.
        """
        self.detach()
        if self.locked:
            self._holds_construction_lock = False
            Context._release_construction_lock()

    @contextmanager
    def construct(self):
        """
        Release the global context once all models have been instantiated in the context.
        Calls elaborate() at the end of construction.
        """
        try:
            yield
        except Exception as e:
            print(f"Exception occurred during context construction: {e}")
            raise
        else:
            # Elaborate at end of construction. This also releases the construction lock.
            self.elaborate()
        finally:
            self.release()


def _wrap_module_init(init):
    """
    Wrap a Module subclass's __init__ so that the outermost call of a construction
    builds the module:

    - Opens a ModuleName scope so ports, signals, and submodules created in __init__
      get hierarchical names, and ends it deterministically when __init__ returns or raises.
    - Initializes the C++ Module exactly once, before any user __init__ code runs.
    - Gives the context ownership of the module so it isn't garbage collected.

    Inner calls (super().__init__ from a subclass) run the wrapped __init__ directly.
    """
    sig = inspect.signature(init)
    if "name" not in sig.parameters:
        raise TypeError(f"{init.__qualname__}() must take a 'name' argument")

    @functools.wraps(init)
    def __init__(self, *args, **kwargs):
        if self._dspsim_constructing:
            init(self, *args, **kwargs)
            return

        bound = sig.bind(self, *args, **kwargs)
        bound.apply_defaults()
        with ModuleName(bound.arguments["name"]) as module_name:
            _Module.__init__(self, module_name)
            self._dspsim_constructing = True
            try:
                init(self, *args, **kwargs)
            finally:
                self._dspsim_constructing = False
        self.context.own_model(self)

    return __init__


class Module(_Module):
    """
    Python base class for modules.

    Subclasses define __init__ with a `name` argument plus any other arguments, and can
    be subclassed further. Calling super().__init__(name) is optional: the C++ Module is
    initialized before the outermost __init__ runs, so self.context and the other Module
    members are usable from its first line.

        class Adder(Module):
            def __init__(self, name: str, width: int = 8):
                super().__init__(name)
                self.a = InputU8("a")
    """

    # True while this instance's __init__ chain is running.
    _dspsim_constructing: bool = False

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        # Subclasses without their own __init__ inherit an already wrapped one.
        if "__init__" in cls.__dict__:
            cls.__init__ = _wrap_module_init(cls.__dict__["__init__"])

    def __init__(self, name: str):
        """Construction is handled by the wrapper, so this does nothing."""


Module.__init__ = _wrap_module_init(Module.__init__)


def signal(name: str, init: int = 0, width: int = 32, is_signed: bool = False):
    if width <= 8:
        return SignalS8(name, width, init) if is_signed else SignalU8(name, width, init)
    elif width <= 16:
        return (
            SignalS16(name, width, init) if is_signed else SignalU16(name, width, init)
        )
    elif width <= 32:
        return (
            SignalS32(name, width, init) if is_signed else SignalU32(name, width, init)
        )
    elif width <= 64:
        return (
            SignalS64(name, width, init) if is_signed else SignalU64(name, width, init)
        )
    else:
        raise ValueError("Unsupported signal width")


__all__ = [
    "Clock",
    "Context",
    "ContextConstructionError",
    "DffS8",
    "DffS16",
    "DffS32",
    "DffS64",
    "DffU8",
    "DffU16",
    "DffU32",
    "DffU64",
    "InputArrayFloat",
    "InputArrayS8",
    "InputArrayS16",
    "InputArrayS32",
    "InputArrayS64",
    "InputArrayU8",
    "InputArrayU16",
    "InputArrayU32",
    "InputArrayU64",
    "InputArrayViewFloat",
    "InputArrayViewS8",
    "InputArrayViewS16",
    "InputArrayViewS32",
    "InputArrayViewS64",
    "InputArrayViewU8",
    "InputArrayViewU16",
    "InputArrayViewU32",
    "InputArrayViewU64",
    "InputFloat",
    "InputS8",
    "InputS16",
    "InputS32",
    "InputS64",
    "InputU8",
    "InputU16",
    "InputU32",
    "InputU64",
    "Model",
    "Module",
    "ModuleName",
    "OutputArrayFloat",
    "OutputArrayS8",
    "OutputArrayS16",
    "OutputArrayS32",
    "OutputArrayS64",
    "OutputArrayU8",
    "OutputArrayU16",
    "OutputArrayU32",
    "OutputArrayU64",
    "OutputArrayViewFloat",
    "OutputArrayViewS8",
    "OutputArrayViewS16",
    "OutputArrayViewS32",
    "OutputArrayViewS64",
    "OutputArrayViewU8",
    "OutputArrayViewU16",
    "OutputArrayViewU32",
    "OutputArrayViewU64",
    "OutputFloat",
    "OutputS8",
    "OutputS16",
    "OutputS32",
    "OutputS64",
    "OutputU8",
    "OutputU16",
    "OutputU32",
    "OutputU64",
    "SignalArrayFloat",
    "SignalArrayS8",
    "SignalArrayS16",
    "SignalArrayS32",
    "SignalArrayS64",
    "SignalArrayU8",
    "SignalArrayU16",
    "SignalArrayU32",
    "SignalArrayU64",
    "SignalArrayViewFloat",
    "SignalArrayViewS8",
    "SignalArrayViewS16",
    "SignalArrayViewS32",
    "SignalArrayViewS64",
    "SignalArrayViewU8",
    "SignalArrayViewU16",
    "SignalArrayViewU32",
    "SignalArrayViewU64",
    "SignalFloat",
    "SignalS8",
    "SignalS16",
    "SignalS32",
    "SignalS64",
    "SignalU8",
    "SignalU16",
    "SignalU32",
    "SignalU64",
    "Wait",
    "WaitBase",
    "WaitSensitivityEvent",
    "WaitTimeEvent",
    "get_global_context_factory",
    # "set_global_context_factory",
    "signal",
]
