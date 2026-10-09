from dspsim.framework import Context


def test_context_obtain():
    context = Context("some_context")
    assert context is not None
    assert isinstance(context, Context)

    # Reset the context and check if a new instance is created
    context.release()

    new_context = Context()
    assert new_context.id != context.id
    assert isinstance(new_context, Context)
    print(new_context)


def test_second_context_in_same_thread_raises():
    import pytest

    from dspsim.framework import ContextConstructionError, SignalU8

    with Context("first") as first:
        with pytest.raises(ContextConstructionError, match="first"):
            Context("second")

        # The first context is still the active one.
        s = SignalU8("s")
        assert s.context.id == first.id


def test_elaborate_detaches_and_allows_new_context():
    from dspsim.framework import SignalU8

    with Context("elab_a") as a:
        sa = SignalU8("sa")
        a.elaborate()
        assert a.elaborated
        assert not a.locked

        with Context("elab_b") as b:
            sb = SignalU8("sb")
            assert sa.context.id == a.id
            assert sb.context.id == b.id
            b.elaborate()


def test_releasing_inactive_context_keeps_active_context():
    import gc

    from dspsim.framework import SignalU8

    a = Context("detached_a")
    a.elaborate()
    b = Context("active_b")
    # Releasing or deleting a detached context must not reset the active context.
    a.release()
    del a
    gc.collect()
    s = SignalU8("s")
    assert s.context.id == b.id
    b.release()


def test_other_thread_waits_for_elaborate():
    import threading

    created = threading.Event()
    result = {}

    def other():
        with Context("other") as ctx:
            created.set()
            result["id"] = ctx.id
            ctx.elaborate()

    with Context("main") as main:
        t = threading.Thread(target=other)
        t.start()
        # The other thread blocks while this context is under construction.
        assert not created.wait(0.2)
        main.elaborate()
        assert created.wait(5)
        t.join(5)
    assert result["id"] != main.id


def test_contexts_with_same_name_coexist():
    with Context("same_name") as a:
        a.elaborate()
        with Context("same_name") as b:
            assert a.id != b.id
            b.elaborate()


def test_create_refuses_unelaborated_implicit_context():
    import pytest
    from dspsim._framework import Context as _Context

    from dspsim.framework import ContextConstructionError, SignalU8

    # A model created without a Context gets an implicit, unelaborated context.
    s = SignalU8("implicit")
    try:
        with pytest.raises(ContextConstructionError, match="has not been elaborated"):
            Context("blocked")
        # The construction lock was released after the failed create.
        assert not Context._construction_lock.locked()
    finally:
        assert s.context is not None
        _Context.reset()
