"""Shared CLI parser types."""

from __future__ import annotations

import argparse
from typing import TYPE_CHECKING, TypeAlias

if TYPE_CHECKING:
    Subcommands: TypeAlias = argparse._SubParsersAction[argparse.ArgumentParser]
else:
    Subcommands: TypeAlias = argparse._SubParsersAction
