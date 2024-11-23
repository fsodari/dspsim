"""DSPSim from pyproject.toml"""

from pathlib import Path
from dataclasses import dataclass, field
import tomllib
import glob

# from .util import cmake_dir
from .generate import ConfigModule, Config
