"""Backward-compat shim — tests moved to:
  test_cli_train.py    — CliTrainTests
  test_cli_outline.py  — CliOutlineTests
  test_cli_prefs.py    — CliPrefsTests
  test_cli_dataset.py  — CliDatasetTests
"""
from tests.test_cli_train import *    # noqa: F401, F403
from tests.test_cli_outline import *   # noqa: F401, F403
from tests.test_cli_prefs import *     # noqa: F401, F403
from tests.test_cli_dataset import *   # noqa: F401, F403
