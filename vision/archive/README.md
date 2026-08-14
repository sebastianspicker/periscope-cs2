# Repository archive

Machine-local and historical material that is not part of the active product
path. Do not import from here at runtime. Being under the Periscope monorepo
does not change this policy: archives stay historical-only.

| Path | Contents |
|------|----------|
| [`docs/archive/`](../docs/archive/) | Superseded design notes, external reviews, and completed living ledgers |
| [`archive/local/`](local/) | Machine-specific configs (absolute paths, personal prefs) |
| [`archive/internal/`](internal/) | Maintainer-only notes (not public product docs) |
| [`archive/tests/`](tests/) | Obsolete test shims kept for reference |
| [`archive/workspace/`](workspace/) | Scratch / diagnostic dumps kept for debugging (if any) |

## Policy

- Prefer archiving over deleting when content may help audits or diagnosis.
- Delete only regenerable caches, OS junk, and local model downloads that
  Ultralytics or `cs2-vision download-model` can recreate.
- Active source, tests, package config data, and `configs/*.example.json` stay
  in the tree root (not here).
- Completed living ledgers (for example LoC or remediation trackers) go under
  `docs/archive/`, not back into active `docs/` topic lists.
