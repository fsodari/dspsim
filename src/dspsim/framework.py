"""
Framework module for dspsim.
This module provides the Context and Model classes for building and simulating models.
"""

from dspsim._framework import Context
from dspsim._framework import Model as _Model


class Model(_Model):
    """
    Use Python models in a simulation. Subclasses of this class MUST call super().__init__() in their constructor.
    """

    def __init__(self):
        super().__init__()
        # Register the model with its context.
        self.context.register_model(self)


__all__ = ["Context", "Model"]
