# nanobind stubgen pattern file for dspsim._framework (see dspsim_add_stub).

# Array indexing: a key made only of ints matches both overloads and the result depends on the array's rank
# (element for a full-rank key, otherwise a view), which the type system cannot express. The overlap is intended.
# Keep these signatures in sync with bind_ndarray_common() in src/include/bindings/ndarray_bindings.h.
\.(Signal|Input|Output)Array(?:View)?(\w+)\.__getitem__$:
    \from typing import overload
    @overload
    def __getitem__(self, key: int | tuple[int, ...], /) -> \1\2: ...  # type: ignore[overload-overlap]

    @overload
    def __getitem__(self, key: slice | int | tuple[int | slice, ...], /) -> \1ArrayView\2: ...

