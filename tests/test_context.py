from dspsim.framework import Context


def test_context_obtain():
    context = Context("some_context")
    assert context is not None
    assert isinstance(context, Context)
    assert context.constructing
    assert Context.obtain().id == context.id

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
        assert not a.constructing

        with Context("elab_b") as b:
            sb = SignalU8("sb")
            assert sa.context.id == a.id
            assert sb.context.id == b.id
            b.elaborate()


def test_models_after_elaboration_raise():
    import pytest

    from dspsim.framework import ContextConstructionError, SignalU8

    with Context("elab_raise") as ctx:
        sa = SignalU8("sa")
        ctx.elaborate()
        with pytest.raises(ContextConstructionError):
            SignalU8("late")
        assert len(ctx.models) == 1
        assert sa.context.id == ctx.id

    # A new context can still be created.
    with Context("after_raise") as ctx:
        SignalU8("sb")
        ctx.elaborate()


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


def test_models_require_a_context():
    import pytest

    from dspsim.framework import ContextConstructionError, SignalU8

    # There is no implicit context: models can only be constructed in an active context.
    with pytest.raises(ContextConstructionError, match="no active context"):
        SignalU8("orphan")

    with Context("after_orphan") as ctx:
        SignalU8("s")
        ctx.elaborate()
