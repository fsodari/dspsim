from dspsim._framework import Wait

from dspsim.framework import (
    Clock,
    Context,
    InputU32,
    Module,
    OutputU32,
    SignalU32,
    # Wait,
    WaitTimeEvent,
)


class SomeCoroModule(Module):
    def __init__(self, name: str):
        super().__init__(name)

        self.proc = self.add_task(self.some_coro())

    async def some_coro(self):
        print("Starting coroutine")
        while True:
            print(f"Waiting for 2 time units, t={self.context.time}")
            await self.wait(2)

            print(f"Waiting for 3 time units, t={self.context.time}")
            await self.wait(3)


def test_some_coro():
    with Context() as ctx:
        ctx.log_level = "debug"
        with ctx.construct():
            clk = Clock("clk", 10)
            m = SomeCoroModule("m")

        ctx.run(20)


class EventWaiter(Module):
    def __init__(self, name: str, a: SignalU32, b: SignalU32, c: SignalU32):
        self.a, self.b, self.c = a, b, c
        self.log: list[str] = []
        self.add_task(self.body())

    async def body(self):
        await self.wait(self.a.change())
        self.log.append("a")
        await self.wait([self.b.change(), self.c.change()])
        self.log.append("b|c")
        await self.wait(self.a.change())
        self.log.append("a again")


def test_py_event_waits():
    with Context() as ctx:
        with ctx.construct():
            a, b, c = SignalU32("a"), SignalU32("b"), SignalU32("c")
            m = EventWaiter("m", a, b, c)
        ctx.run(1)
        assert m.log == []

        # A single event wait wakes the task.
        a.write(1)
        ctx.run(1)
        assert m.log == ["a"]

        # Waiting on a list wakes on either event.
        c.write(1)
        ctx.run(1)
        assert m.log == ["a", "b|c"]

        # b was part of the previous wait, so it must not wake the task now.
        b.write(1)
        ctx.run(1)
        assert m.log == ["a", "b|c"]

        a.write(2)
        ctx.run(1)
        assert m.log == ["a", "b|c", "a again"]
