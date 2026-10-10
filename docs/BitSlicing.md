# Bit Slicing and Packing

Signals can be sliced and concatenated by bit, as in SystemVerilog, and ports can bind to the result.
Typical uses are splitting a bus into fields, or building a wide port such as `tdata` out of several signals.

```cpp
Signal<int16_t> tdata{"tdata", 16};
Signal<uint8_t> tid{"tid"};
Signal<int32_t> m_tdata{"m_tdata", 24};

skid.s_axis_tdata.bind(pack(tdata, tid));   // {tdata, tid} drives a 24-bit input
skid.m_axis_tdata.bind(m_tdata);

auto data = m_tdata[{23, 8}];               // m_tdata[23:8]
uint64_t raw = data.read();                 // slices are unsigned
int64_t value = data.read_signed();         // two's complement of the slice width
```

```python
skid.s_axis_tdata.bind(pack(tdata, tid))
data = m_tdata[23:8]
value = data.read_signed()
```

## Bit selections (`BitSel`)

A `BitSel` (`dspsim/bitsel.h`) is a lightweight handle to some bits of one or more signals. It stores no value.

| C++ | Python | Meaning |
|---|---|---|
| `sig.slice(hi, lo)`, `sig[{hi, lo}]` | `sig.slice(hi, lo)`, `sig[hi:lo]` | bits `[hi:lo]`, inclusive |
| `sig[bit]` | `sig[bit]` | one bit |
| | `sig[:lo]`, `sig[hi:]` | to the top / bottom bit |
| `pack(a, b, ...)` | `pack(a, b, ...)` | `{a, b, ...}`, first argument most significant |
| `BitSel::concat(vector)` | | `pack` without templates |

- `read()` returns the selected bits of the committed values as an unsigned `uint64_t`. `read_signed()` interprets them as a two's complement number of the selection's width.
- `write(v)` updates the pending value of each signal involved, bits of `v` above the width are ignored. Writes to different bits of the same signal in the same cycle combine. In Python, `write` accepts any int, and negative values are written as two's complement.
- A slice of a pack, or a pack of slices, refers directly to the underlying signals. Adjacent ranges of the same signal are merged. `parts()` lists the ranges, least significant first.
- `pack` accepts signals, selections, `SignalArray`s and array views. Arrays are packed in row-major order with element 0 most significant, like `{arr[0], arr[1], ...}`. In Python, any iterable (such as a list) is flattened the same way.
- Selections are at most 64 bits wide, and only integral signals can be selected. Errors throw `std::out_of_range` (bad range, `IndexError` in Python) or `std::invalid_argument` (non-integral signal or too wide, `ValueError` in Python).
- Signals convert implicitly to a whole-signal `BitSel`.

## Binding ports

`Input<T>::bind(const BitSel &)` and `Output<T>::bind(const BitSel &)` exist for every integral `T`. The selection must have the port's width, and the bind must happen before elaboration.

Binding creates a `DerivedSignal<T>` (`dspsim/derived_signal.h`) that holds the selection's value as a `T`, so ports read it at full speed. Signed types are sign extended from the selection width.

- **Inputs** see changes in the same delta cycle as the source signals, and notify once even when several sources change together. Edge events follow the selected bits only: a port bound to `a[3]` sees a posedge only when bit 3 rises.
- **Outputs** write through to the source signals' pending values at write time. Writes to different slices of one signal, or to a slice and the whole signal, combine like SystemVerilog nonblocking assignments: the last write wins per bit, and the signal commits once.

Because signals convert implicitly, a port can also bind to a signal of a different type with the same width, for example `Input<uint32_t>` of width 8 to a `Signal<uint8_t>`.

## Signals from selections

`sel.signal(name = "")` creates a derived signal for a selection without binding a port, for example to be sensitive to a single bit:

```cpp
SignalBase &bit3 = a[3].signal("bit3");
DSPSIM_METHOD(eval)->always(bit3.pos());

DerivedSignal<int16_t> &field = a[{11, 0}].signal<int16_t>();  // typed, sign extended
```

The untyped version uses the smallest unsigned type that fits (`DerivedSignalU8` ... `DerivedSignalU64` in `dspsim._framework`). Each call creates a new context-owned signal, and it must be called before elaboration. A derived signal has no value of its own: `init()` throws, and `write()` goes through to the sources.

## Bit helpers (`dspsim/bits.h`)

| Function | Result |
|---|---|
| `mask(width)` | lower `width` bits set |
| `bits(value, hi, lo)` | `value[hi:lo]` shifted down to bit 0 |
| `zext(value, width)` | lower `width` bits of `value` |
| `sext(value, width)` | lower `width` bits of `value`, sign extended to `int64_t` |
| `mask<W, U>()`, `sext<W>(value)` | compile-time width versions for C++ |

The runtime versions are available from Python. They validate the width and raise `ValueError` when it is out of range (`IndexError` for a bad range in `bits`).

## How it works

- Every signal keeps a list of the derived signals built from it. When it commits a change in `Signal<T>::update()`, it queues them on `Context::_derived_update_stack`. Signals without dependents pay one empty-vector check.
- The update phase (`Context::_update_signals()`) first drains the signal update stack, then recomputes the queued derived signals. This way a derived signal always sees all of its sources' new values and changes in the same delta cycle as them.
- A derived signal is a view. `Signal<T>::write()` checks `source_selection()` (one pointer test, no virtual call) and writes the bits through to the sources instead of scheduling itself. The sources commit in the update phase and the derived signal reads the new value back from them.
- Selections are flattened, so derived signals only ever depend on ordinary signals. Selecting bits of a derived signal selects the bits of its sources.
- Derived signals are owned by the context and don't unregister from their sources, so the sources must outlive the simulation.
