"""
The values of signals and ports whose type Python does not have a class for: structs, arrays, and the
numbers that are fields of them. The bindings (value_bindings.h) describe a value type with nested tuples and
pass the bytes of a value; this module makes numpy values of them and back.

A description is `(kind, size, shape)` for a number, with kind "b" (bool), "u", "i" or "f" and the size in
bytes, or `("S", name, size, fields, shape)` for a struct, where fields is a tuple of `(name, offset,
description)`. `shape` is that of an array of such elements, `()` for one.

A struct is a numpy structured value (a Record) whose dtype has the fields at the offsets that the C++
compiler gave them, padding included, so the bytes are those of the C++ struct.
"""

import functools
from collections.abc import Mapping, Sequence

import numpy as np

_FORMATS = {"b": "?", "u": "u{}", "i": "i{}", "f": "f{}"}

# The attributes of a numpy.record that are not fields, whatever the fields are called: numpy.record needs them.
_RECORD_ATTRIBUTES = ("dtype", "getfield", "setfield")


# numpy's stubs pin `__module__` and the overloads of `__getitem__` of numpy.record, which a subclass changes.
class Record(np.record):  # pyright: ignore[reportIncompatibleVariableOverride]
    """
    The value of a struct: a numpy.record whose fields come first as attributes.

    A field is an attribute and an item, `value.data` and `value["data"]`, to read and to assign. A
    numpy.record gives its own attributes priority over fields, so that `value.data`, `value.real` or
    `value.size` are not the fields of those names; here they are. What a field hides is still there as a
    function of numpy (`np.copy(value)` for `value.copy()`). Only `dtype`, `getfield` and `setfield` are
    never fields as attributes: a field of such a name is an item.

    A nested struct is a Record too and an array a numpy array, both views of the value: assigning to
    `value.iq.re` or `value.taps[0]` changes the value.
    """

    def __getattribute__(self, name: str):
        if name not in _RECORD_ATTRIBUTES:
            fields = np.void.__getattribute__(self, "dtype").fields
            if fields is not None and name in fields:
                return _as_records(self[name])
        return super().__getattribute__(name)

    def __getitem__(self, key):  # pyright: ignore[reportIncompatibleMethodOverride]
        return _as_records(super().__getitem__(key))


def _as_records(value):
    """An array of structs with Record as the type of its elements. Anything else as it is."""
    if isinstance(value, np.ndarray) and value.dtype.names is not None:
        return value.view((Record, value.dtype))
    return value


@functools.cache
def element_dtype(description: tuple) -> np.dtype:
    """The numpy dtype of one element of a described value."""
    if description[0] != "S":
        kind, size, _ = description
        return np.dtype(_FORMATS[kind].format(size))
    _, _, size, fields, _ = description
    return np.dtype(
        {
            "names": [name for name, _, _ in fields],
            "formats": [dtype_of(field) for _, _, field in fields],
            "offsets": [offset for _, offset, _ in fields],
            "itemsize": size,
        }
    )


def dtype_of(description: tuple) -> np.dtype:
    """The numpy dtype of a described value: that of an element, with the shape of an array."""
    element, shape = element_dtype(description), description[-1]
    return np.dtype((element, shape)) if shape else element


def from_bytes(description: tuple, data: bytes, shape: tuple[int, ...] = ()):
    """
    The value that the bytes are, as a copy that can be changed and written back. `shape` is the shape of an
    array of signals or ports whose values the bytes are, one after the other.

    A struct is a Record (a numpy.record: fields are attributes and items), an array a numpy array, whose
    elements are Records if they are structs and whose fields are items (`values["re"]`), and a number a
    Python number.
    """
    element = element_dtype(description)
    full_shape = (*shape, *description[-1])
    array = _as_records(
        np.frombuffer(bytearray(data), dtype=element).reshape(full_shape)
    )
    if full_shape:
        return array
    return array[()] if element.names is not None else array.item()


def _convert(dtype: np.dtype, shape: tuple[int, ...], value) -> np.ndarray:
    """`value` as an array of that element dtype and shape. Structs can be mappings, by field name."""
    # A tuple is one struct, as in numpy: only a list is an array of structs.
    one_struct = dtype.names is not None and isinstance(value, (tuple, Mapping))
    is_numpy = isinstance(value, (np.ndarray, np.generic))
    if (
        shape
        and not one_struct
        and not is_numpy
        and isinstance(value, Sequence)
        and len(value) == shape[0]
    ):
        return np.stack([_convert(dtype, shape[1:], item) for item in value])
    if isinstance(value, Mapping):
        if dtype.names is None:
            raise TypeError("a mapping is the value of a struct, not of a number")
        if missing := [name for name in dtype.names if name not in value]:
            raise ValueError(f"the value has no field '{missing[0]}'")
        if unknown := [name for name in value if name not in dtype.names]:
            raise ValueError(f"the struct has no field '{unknown[0]}'")
        array = np.zeros((), dtype)
        for name in dtype.names:
            field = dtype[name]
            base, field_shape = field.subdtype if field.subdtype else (field, ())
            array[name] = _convert(base, field_shape, value[name])
        return array if not shape else np.broadcast_to(array, shape)
    array = np.asarray(value, dtype=dtype)
    return array if array.shape == shape else np.broadcast_to(array, shape)


def to_bytes(description: tuple, value, shape: tuple[int, ...] = ()) -> bytes:
    """
    The bytes of a value: a numpy value of the dtype, or anything numpy converts to it (a tuple with the
    fields of a struct in order, nested lists for an array), or a mapping with every field of a struct by
    name. A value with fewer dimensions than `shape` is written to every signal or port of the array.
    """
    element = element_dtype(description)
    array = _convert(element, (*shape, *description[-1]), value)
    return np.ascontiguousarray(array).tobytes()
