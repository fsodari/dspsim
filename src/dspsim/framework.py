import atexit
import functools
import inspect
from contextlib import contextmanager

from dspsim._framework import (
    AxisRxS8,
    AxisRxS16,
    AxisRxS32,
    AxisRxS64,
    AxisRxU8,
    AxisRxU16,
    AxisRxU32,
    AxisRxU64,
    AxisTxS8,
    AxisTxS16,
    AxisTxS32,
    AxisTxS64,
    AxisTxU8,
    AxisTxU16,
    AxisTxU32,
    AxisTxU64,
    BitSel,
    Channel,
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
    FieldSignalFloat,
    FieldSignalS8,
    FieldSignalS16,
    FieldSignalS32,
    FieldSignalS64,
    FieldSignalU8,
    FieldSignalU16,
    FieldSignalU32,
    FieldSignalU64,
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
    PortBase,
    PortBaseArrayView,
    PortStruct,
    PortStructArray,
    RecordPort,
    RecordSignal,
    SensitivityEvent,
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
    SignalBase,
    SignalBaseArrayView,
    SignalFloat,
    SignalS8,
    SignalS16,
    SignalS32,
    SignalS64,
    SignalStruct,
    SignalStructArray,
    SignalU8,
    SignalU16,
    SignalU32,
    SignalU64,
    Task,
    TaskBool,
    TaskListS8,
    TaskListS16,
    TaskListS32,
    TaskListS64,
    TaskListU8,
    TaskListU16,
    TaskListU32,
    TaskListU64,
    Wait,
    WaitResult,
    WaitSensitivityEvent,
    WaitTimeEvent,
    _reset_global_context_factory,
    bits,
    mask,
    pack,
    sext,
    zext,
)
from dspsim._framework import Context as _Context
from dspsim._framework import Module as _Module
from dspsim._values import Record

# Prevent nb leak warnings.
atexit.register(_reset_global_context_factory)


class Context(_Context):
    """
    Python wrapper for the C++ Context class.

    Only one context can be under construction at a time, because models register with the
    global active context. Creating a Context takes a global construction lock (in C++) that is
    released when the context is elaborated or released. After elaboration the design is locked,
    the context is released from the global factory, and it can simulate independently of new contexts.

    - Another thread creating a Context blocks until the current one is elaborated or released.
    - The same thread creating a second Context before elaborating the first would deadlock,
      so it raises ContextConstructionError instead.
    """

    def __del__(self):
        self.release()

        # Calling clear here prevents nanobind leak warnings.
        self.clear()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.clear()
        self.release()

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
        self.context._own_object(self)

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
        # More than 64 bits: an array of 32-bit words, least significant first, as verilated ports of that
        # width are. Read and write it as one number with from_words() and to_words().
        if init != 0:
            raise ValueError("A signal wider than 64 bits can only be initialized to 0")
        return SignalArrayU32(name, (-(-width // WORD_BITS),))


# Bits in one word of a signal or port wider than 64 bits.
WORD_BITS = 32


def to_words(value: int, count: int) -> list[int]:
    """
    Split an integer into `count` 32-bit words, least significant first. This is how dspsim holds a number of
    more than 64 bits: in an array of 32-bit signals or ports, like the ports of a verilated model.

        a = SignalArrayU32("a", model.a.shape)
        a.d = to_words(value, len(a))

    Negative values are written as two's complement. Raises OverflowError if the value does not fit.
    """
    bits = WORD_BITS * count
    if not -(1 << bits) // 2 <= value < 1 << bits:
        raise OverflowError(
            f"{value} does not fit in {count} words of {WORD_BITS} bits"
        )
    mask = (1 << WORD_BITS) - 1
    return [(value >> (WORD_BITS * i)) & mask for i in range(count)]


def from_words(words, *, width: int | None = None, signed: bool = False) -> int:
    """
    Combine 32-bit words, least significant first, into one integer: the opposite of to_words(). `words` is
    any iterable of integers, such as the value of a one-dimensional array of signals or ports.

        value = from_words(a.q)

    `width` is the number of bits of the value, by default all the bits of the words. Bits above it are
    dropped, and with `signed` the value is read as a two's complement number of that width.
    """
    words = [int(word) for word in words]
    bits = WORD_BITS * len(words)
    if width is None:
        width = bits
    elif not 0 < width <= bits:
        raise ValueError(f"width must be between 1 and {bits} for {len(words)} words")
    mask = (1 << WORD_BITS) - 1
    value = sum((word & mask) << (WORD_BITS * i) for i, word in enumerate(words))
    value &= (1 << width) - 1
    if signed and value >> (width - 1):
        value -= 1 << width
    return value


__all__ = [
    "AxisRxS8",
    "AxisRxS16",
    "AxisRxS32",
    "AxisRxS64",
    "AxisRxU8",
    "AxisRxU16",
    "AxisRxU32",
    "AxisRxU64",
    "AxisTxS8",
    "AxisTxS16",
    "AxisTxS32",
    "AxisTxS64",
    "AxisTxU8",
    "AxisTxU16",
    "AxisTxU32",
    "AxisTxU64",
    "BitSel",
    "Channel",
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
    "FieldSignalFloat",
    "FieldSignalS8",
    "FieldSignalS16",
    "FieldSignalS32",
    "FieldSignalS64",
    "FieldSignalU8",
    "FieldSignalU16",
    "FieldSignalU32",
    "FieldSignalU64",
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
    "PortBase",
    "PortBaseArrayView",
    "PortStruct",
    "PortStructArray",
    "Record",
    "RecordPort",
    "RecordSignal",
    "SensitivityEvent",
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
    "SignalBase",
    "SignalBaseArrayView",
    "SignalFloat",
    "SignalS8",
    "SignalS16",
    "SignalS32",
    "SignalS64",
    "SignalStruct",
    "SignalStructArray",
    "SignalU8",
    "SignalU16",
    "SignalU32",
    "SignalU64",
    "Task",
    "TaskBool",
    "TaskListS8",
    "TaskListS16",
    "TaskListS32",
    "TaskListS64",
    "TaskListU8",
    "TaskListU16",
    "TaskListU32",
    "TaskListU64",
    "Wait",
    "WaitResult",
    "WaitSensitivityEvent",
    "WaitTimeEvent",
    "bits",
    "from_words",
    "mask",
    "pack",
    "sext",
    "signal",
    "to_words",
    "zext",
]
