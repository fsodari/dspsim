from dspsim._framework import Context  # type: ignore
from dspsim._framework import Model as _Model  # type: ignore


class Model(_Model):
    def __init__(self):
        super().__init__()
        # Python models can't self register. (can't call shared_from_this in constructor)
        self.context.register_model(self)


__all__ = ["Context", "Model"]
