import atexit
import functools
import threading
from contextlib import contextmanager

from dspsim._framework import (
    Clock,
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
        Calls elaborate() at the end of construction.
        """
        try:
            yield
        except Exception as e:
            print(f"Exception occurred during context construction: {e}")
            raise
        else:
            # Elaborate at end of construction
            self.elaborate()
        finally:
            # Release the global context lock after elaboration
            self.release()


class Module(_Module):
    """
    Python wrapper for the C++ Module class.

    Initialization order:
        - Subclass __init__ is replaced with new_init. new_init called.
        - new_init calls _Module __init__
        - Subclass calls Module.__init__ with super().__init__
        - Subclass __init__ completes.
        - new_init completes.

    This is done so the subclass doesn't deal with ModuleName directly.
    """

    def __init_subclass__(cls):
        original_init = cls.__init__

        @functools.wraps(original_init)
        def __new_init__(self, name: str):
            # Get a new ModuleName instance for the module.
            _name = ModuleName(name)

            # Calls _Module.__init__
            super().__init__(_name)
            original_init(self, name)
            # Deleting _name is important to change the context's active module.
            del _name

        # Subclasses's init will now handle ModuleName properly.
        cls.__init__ = __new_init__

    def __init__(self, name: str):
        """Subclass must call super().__init__ so context.own_model(self) gets called."""
        self.context.own_model(self)


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
