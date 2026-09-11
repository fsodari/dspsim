from dspsim.framework import Context


def test_context_obtain():
    context = Context()
    assert context is not None
    assert isinstance(context, Context)

    print(context)

    # Ensure obtaining another context returns the same instance
    another_context = Context()
    assert context is another_context

    # Reset the context and check if a new instance is created
    context.reset()
    new_context = Context()
    assert new_context is not context
    assert isinstance(new_context, Context)
    print(new_context)
