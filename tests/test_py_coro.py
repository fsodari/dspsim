from dspsim._framework import Wait

from dspsim.framework import (
    Clock,
    Context,
    Input32,
    Module,
    Output32,
    Signal32,
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
