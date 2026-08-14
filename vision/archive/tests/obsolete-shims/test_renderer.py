# Backward-compat shim — tests moved to:
#   test_renderer_outline.py  — OutlineStyleTests
#   test_renderer_roles.py    — RoleTreatmentCatalogTests, RoleRendererTests
from tests.test_renderer_outline import *  # noqa: F401, F403
from tests.test_renderer_roles import *    # noqa: F401, F403
