import pytest

from dspsim.framework import (
    Context,
    SignalArrayU32,
    SignalS32,
    SignalU8,
    SignalU16,
    SignalU64,
    from_words,
    signal,
    to_words,
)


def test_signal_creation():
    with Context() as ctx:
        s8 = signal("s8", init=1, width=8)
        assert s8.context.id == ctx.id
        assert isinstance(s8, SignalU8)

        s16 = signal("s16", init=1, width=16)
        assert s16.context.id == ctx.id
        assert isinstance(s16, SignalU16)

        s32 = signal("s32", init=1, width=32, is_signed=True)
        assert s32.context.id == ctx.id
        assert isinstance(s32, SignalS32)

        s64 = signal("s64", init=1, width=64)
        assert s64.context.id == ctx.id
        assert isinstance(s64, SignalU64)

        # More than 64 bits: an array of 32-bit words.
        s100 = signal("s100", width=100)
        assert s100[0].context.id == ctx.id
        assert isinstance(s100, SignalArrayU32)
        assert s100.shape == (4,)

        with pytest.raises(ValueError, match="can only be initialized to 0"):
            signal("s128", init=1, width=128)

        # print(ctx.models)


def test_words():
    """Numbers of more than 64 bits are held as 32-bit words, least significant first."""
    value = 0x9_00000003_00000002_FFFFFFFF
    assert to_words(value, 4) == [0xFFFFFFFF, 2, 3, 9]
    assert from_words([0xFFFFFFFF, 2, 3, 9]) == value
    assert to_words(0, 3) == [0, 0, 0]

    # Negative values are two's complement over all the words.
    assert to_words(-1, 4) == [0xFFFFFFFF] * 4
    assert to_words(-2, 3) == [0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF]
    assert from_words(to_words(-12345, 4), signed=True) == -12345

    # A width drops the bits of the top word above it, and gives the position of the sign.
    assert from_words([0xFFFFFFFF] * 4, width=100) == (1 << 100) - 1
    assert from_words([0xFFFFFFFF] * 4, width=100, signed=True) == -1
    assert from_words([0, 0, 0, 0x8], width=100, signed=True) == -(1 << 99)
    assert from_words([0, 0, 0, 0x7], width=100, signed=True) == 7 << 96

    # Range checks.
    assert to_words((1 << 96) - 1, 3) == [0xFFFFFFFF] * 3
    assert to_words(-(1 << 95), 3) == [0, 0, 0x80000000]
    with pytest.raises(OverflowError):
        to_words(1 << 96, 3)
    with pytest.raises(OverflowError):
        to_words(-(1 << 95) - 1, 3)
    with pytest.raises(ValueError):
        from_words([0, 0], width=65)

    # The value of a signal array converts both ways.
    with Context() as ctx:
        a = signal("a", width=100)
        ctx.elaborate()
        a.d = to_words(value, len(a))
        ctx.run(0)
        assert from_words(a.q) == value
