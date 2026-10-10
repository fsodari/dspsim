"""Asynchronous operations from Python: run_until, waits with timeouts, awaiting C++ tasks."""

import builtins
from dataclasses import dataclass

import pytest

from dspsim.framework import (
    AxisRxS32,
    AxisTxS32,
    Clock,
    Context,
    Module,
    SignalS32,
    SignalU8,
    TimeoutError,
    WaitResult,
)


@dataclass
class AxisLoop:
    """An AxisTx wired straight into an AxisRx."""

    clk: Clock
    rst: SignalU8
    tx: AxisTxS32
    rx: AxisRxS32
    tdata: SignalS32
    tvalid: SignalU8
    tready: SignalU8

    @classmethod
    def build(cls):
        clk = Clock("clk", 10)
        rst = SignalU8("rst")
        tx = AxisTxS32("tx")
        rx = AxisRxS32("rx")
        tdata, tvalid, tready = (
            SignalS32("tdata"),
            SignalU8("tvalid"),
            SignalU8("tready"),
        )
        for m in (tx, rx):
            m.clk.bind(clk)
            m.rst.bind(rst)
        tx.m_axis_tdata.bind(tdata)
        tx.m_axis_tvalid.bind(tvalid)
        tx.m_axis_tready.bind(tready)
        rx.s_axis_tdata.bind(tdata)
        rx.s_axis_tvalid.bind(tvalid)
        rx.s_axis_tready.bind(tready)
        return cls(clk, rst, tx, rx, tdata, tvalid, tready)


def test_run_until_awaits_a_cpp_task_from_a_python_coroutine():
    with Context() as ctx:
        with ctx.construct():
            loop = AxisLoop.build()

        async def main():
            loop.rx.ready = 1
            assert await loop.tx.send([1, -2, 3, -4], timeout=1000)
            data = await loop.rx.receive(4, timeout=1000)
            return data

        assert ctx.run_until(main()) == [1, -2, 3, -4]
        assert ctx.time < 1000
        assert len(loop.rx) == 0


def test_run_until_accepts_a_cpp_task_directly():
    with Context() as ctx:
        with ctx.construct():
            loop = AxisLoop.build()
        loop.rx.ready = 1
        loop.tx.push_range([5, 6, 7])
        # Fewer samples than requested means the receive timed out.
        assert ctx.run_until(loop.rx.receive(5, timeout=200)) == [5, 6, 7]
        assert ctx.time == 200


def test_run_until_timeout_raises_and_the_simulation_stays_usable():
    with Context() as ctx:
        with ctx.construct():
            loop = AxisLoop.build()
        loop.rx.ready = 1

        with pytest.raises(TimeoutError):
            ctx.run_until(loop.rx.receive(1), timeout=100)
        assert ctx.time == 100

        # The builtin TimeoutError catches it too.
        with pytest.raises(builtins.TimeoutError):
            ctx.run_until(loop.rx.receive(1), timeout=100)

        loop.tx.push(42)
        assert ctx.run_until(loop.rx.receive(1, timeout=100)) == [42]


def test_run_until_event_with_timeout():
    with Context() as ctx:
        with ctx.construct():
            loop = AxisLoop.build()
        loop.rx.ready = 1
        assert not ctx.run_until(loop.rx.received, timeout=100)
        assert ctx.time == 100
        loop.tx.push(1)
        assert ctx.run_until(loop.rx.received, timeout=100)
        assert loop.rx.data == [1]


def test_run_until_propagates_exceptions():
    with Context() as ctx:
        with ctx.construct():
            _clk = Clock("clk", 10)

        async def main():
            await ctx.wait(5)
            raise ValueError("task boom")

        with pytest.raises(ValueError, match="task boom"):
            ctx.run_until(main())
        assert ctx.time == 5


def test_run_until_stall():
    with Context() as ctx:
        with ctx.construct():
            s = SignalU8("s")
        with pytest.raises(RuntimeError, match="stalled"):
            ctx.run_until(s.change())


class TimeoutWaiter(Module):
    def __init__(self, name: str, e: SignalU8, f: SignalU8, timeout: int):
        self.e, self.f, self.timeout = e, f, timeout
        self.results: list[WaitResult] = []
        self.wakeups = 0
        self.add_task(self.body())

    async def body(self):
        self.results.append(await self.wait(self.e.change(), timeout=self.timeout))
        self.wakeups += 1
        await self.wait(self.f.change())
        self.wakeups += 1


def test_wait_with_timeout_triggered():
    with Context() as ctx:
        with ctx.construct():
            e, f = SignalU8("e"), SignalU8("f")
            m = TimeoutWaiter("m", e, f, 100)
        ctx.run(10)
        e.write(1)
        ctx.run(10)
        assert m.results == [WaitResult.Triggered]
        assert m.wakeups == 1
        # The stale timeout must not wake the task, which now waits on f.
        ctx.run(200)
        assert m.wakeups == 1
        f.write(1)
        ctx.run(10)
        assert m.wakeups == 2


def test_wait_with_timeout_elapsed():
    with Context() as ctx:
        with ctx.construct():
            e, f = SignalU8("e"), SignalU8("f")
            m = TimeoutWaiter("m", e, f, 100)
        ctx.run(110)
        assert m.results == [WaitResult.Timeout]
        assert m.wakeups == 1
        # e no longer wakes the task.
        e.write(1)
        ctx.run(10)
        assert m.wakeups == 1
        f.write(1)
        ctx.run(10)
        assert m.wakeups == 2


class Receiver(Module):
    """A Python module that awaits a C++ task from its own process."""

    def __init__(self, name: str, rx: AxisRxS32):
        self.rx = rx
        self.received: list[int] = []
        self.add_task(self.body())

    async def body(self):
        self.rx.ready = 1
        while True:
            self.received += await self.rx.receive(2)


def test_module_task_awaits_cpp_task():
    with Context() as ctx:
        with ctx.construct():
            loop = AxisLoop.build()
            m = Receiver("m", loop.rx)
        loop.tx.push_range([1, 2, 3])
        ctx.run(200)
        # Two received, the third waits for a partner.
        assert m.received == [1, 2]
        loop.tx.push(4)
        ctx.run(100)
        assert m.received == [1, 2, 3, 4]
