"""Channels written in Python (state that is committed in the update phase) and the events they notify."""

import gc
import weakref

import pytest

from dspsim.framework import Channel, Clock, Context, Module, SensitivityEvent


class Mailbox(Channel):
    """A value that is written in one delta cycle and can be read in the next."""

    def __init__(self, context: Context):
        super().__init__(context)
        self.written = SensitivityEvent(context)
        self.updates = 0
        self._value = 0
        self._next = 0

    def put(self, value: int):
        self._next = value
        self.request_update()

    def get(self) -> int:
        return self._value

    def update(self):
        self._value = self._next
        self.updates += 1
        self.written.notify()


@pytest.mark.parametrize("writer_first", [True, False])
def test_a_channel_is_updated_in_the_update_phase(writer_first: bool):
    # The writer and the reader are evaluated on the same clock edge, in either order. The reader must see
    # the value from before the edge in both: what the writer puts is committed after the evaluations.
    with Context() as ctx:
        with ctx.construct():
            clk = Clock("clk", 10)
            m = Module("m")
            box = Mailbox(ctx)
            count = [0]
            seen: list[int] = []

            def write():
                count[0] += 1
                box.put(count[0])

            def read():
                seen.append(box.get())

            for func in (write, read) if writer_first else (read, write):
                m.process(func).always(clk.pos()).initialize(False)

        ctx.run(25)
        assert seen == [0, 1, 2]
        assert box.get() == 3
        assert box.updates == 3
        assert box.context.id == ctx.id


def test_a_channel_is_updated_once_however_often_it_is_requested():
    with Context() as ctx:
        with ctx.construct():
            m = Module("m")
            box = Mailbox(ctx)

            def write():
                for value in (1, 2, 3):
                    box.put(value)

            m.process(write)

        ctx.run(0)
        assert box.updates == 1
        assert box.get() == 3


def test_an_update_requested_between_runs_happens_in_the_next_run():
    with Context() as ctx:
        with ctx.construct():
            box = Mailbox(ctx)

        ctx.run(0)
        box.put(5)
        assert box.get() == 0
        ctx.run(0)
        assert box.get() == 5
        assert box.updates == 1


def test_an_event_notified_in_update_wakes_a_waiting_task():
    with Context() as ctx:
        with ctx.construct():
            m = Module("m")
            box = Mailbox(ctx)

        received: list[tuple[int, int]] = []

        async def reader():
            for _ in range(2):
                await m.wait(box.written)
                received.append((ctx.time, box.get()))

        async def writer():
            await m.wait(15)
            box.put(7)
            await m.wait(10)
            box.put(8)

        m.add_task(reader())
        m.add_task(writer())
        ctx.run(100)
        assert received == [(15, 7), (25, 8)]


def test_the_context_keeps_a_channel_alive():
    # A channel that asked for an update must still exist when the update runs, also when Python has
    # dropped its last reference to it.
    log: list[str] = []

    class Recorder(Channel):
        def update(self):
            log.append("updated")

    with Context() as ctx:
        with ctx.construct():
            recorder = Recorder(ctx)

        recorder.request_update()
        alive = weakref.ref(recorder)
        del recorder
        gc.collect()
        assert alive() is not None

        ctx.run(0)
        assert log == ["updated"]

    # Clearing the context, here at the end of the with block, releases it.
    gc.collect()
    assert alive() is None


def test_a_channel_must_implement_update():
    class Incomplete(Channel):
        pass

    with Context() as ctx:
        with ctx.construct():
            channel = Incomplete(ctx)

        channel.request_update()
        with pytest.raises(RuntimeError, match="update"):
            ctx.run(0)
        with pytest.raises(NotImplementedError):
            channel.update()


def test_an_exception_in_update_reaches_the_caller_of_run():
    class Faulty(Mailbox):
        def update(self):
            if self._next < 0:
                raise ValueError("negative")
            super().update()

    with Context() as ctx:
        with ctx.construct():
            box = Faulty(ctx)

        box.put(-1)
        with pytest.raises(ValueError, match="negative"):
            ctx.run(0)

        # The simulation goes on afterwards.
        box.put(4)
        ctx.run(0)
        assert box.get() == 4


def test_an_event_created_in_python_wakes_its_subscribers():
    with Context() as ctx:
        with ctx.construct():
            m = Module("m")
            event = SensitivityEvent(ctx)
            evaluations = [0]

            def subscriber():
                evaluations[0] += 1

            m.process(subscriber).always(event).initialize(False)

        woken: list[int] = []

        async def waiter():
            await m.wait(event)
            woken.append(ctx.time)

        async def notifier():
            await m.wait(20)
            event.notify()
            # Notified twice in a delta cycle: the event still occurs once.
            event.notify()

        m.add_task(waiter())
        m.add_task(notifier())
        ctx.run(50)
        assert woken == [20]
        assert evaluations[0] == 1

        # Notified between two runs: the event occurs in the next run.
        event.notify()
        assert evaluations[0] == 1
        ctx.run(0)
        assert evaluations[0] == 2


def test_the_context_keeps_an_event_alive():
    # A process refers to the events it is sensitive to. An event with no Python reference left must not
    # leave it with a pointer to nothing.
    with Context() as ctx:
        with ctx.construct():
            m = Module("m")
            evaluations = [0]

            def subscriber():
                evaluations[0] += 1

            process = (
                m.process(subscriber).always(SensitivityEvent(ctx)).initialize(False)
            )

        gc.collect()
        ctx.run(10)
        process.disable()
        process.enable()
        ctx.run(10)
        assert evaluations[0] == 0
