# Archive note — monolith refactor (2026-08-09)

**What:** Inventory and notes from splitting every project-owned source (`.cpp`, `.c`, `.h`, `.hpp`, `.py`) that exceeded 600 physical lines.

**Outcome:** Baseline had 23 files over the cap; after the pass, zero remained. Splits kept public façades (`sim/world.hpp`, `gui.hpp`, etc.) where callers already depended on them.

**Live status:** The tree already reflects the split. This ledger is historical — do not reopen it as a todo list unless measuring LoC again.

**See:** `MONOLITH_REFACTOR_LEDGER.md` in this folder.
