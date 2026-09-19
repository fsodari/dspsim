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
