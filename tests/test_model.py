from dspsim.framework import Context, Model


class SomeModel(Model):
    def __init__(self):
        # Must call!
        super().__init__()
        self.eval_step_called = 0
        self.eval_end_step_called = 0

    def eval_step(self):
        self.eval_step_called += 1

    def eval_end_step(self):
        self.eval_end_step_called += 1


def test_model_initialization():
    with Context() as context:
        some_model = SomeModel()
        assert some_model is not None
        assert isinstance(some_model, Model)
        assert some_model.context is not None
        assert isinstance(some_model.context, Context)

        amodel = SomeModel()
        assert amodel is not None
        assert isinstance(amodel, Model)

        assert (
            some_model.context is context
        )  # Ensure the model's context is the same as the global

        assert len(context.models) == 2  # Ensure the model is registered in the context

        # Elaborate and detach context.
        context.elaborate()

        N = 5
        for _ in range(N):
            context.eval()
        assert some_model.eval_step_called == N
        assert some_model.eval_end_step_called == N
        assert amodel.eval_step_called == N
        assert amodel.eval_end_step_called == N
