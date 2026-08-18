# Release Status

Historical evidence date: 2026-08-14

Status: historical local simulation and CPU validation; not publishable from
that snapshot

## Repository boundary

The 2026-08-14 audit described an unborn `main` repository with no commit,
canonical revision, or configured remote. That description is historical and
must not be used as a statement of the current checkout. The local results
below are useful implementation evidence, but they cannot identify a release
candidate and do not authorize a commit, push, tag, package, or deployment.

Periscope contains two separately licensed projects rather than one releasable
package:

- `radar/` is an MIT-licensed C++ educational red/blue simulation lab.
- `vision/` is an AGPL-3.0-only Python accessibility-research package.

## 2026-08-14 local evidence

<!-- markdownlint-disable MD013 -->

| Area | Result |
| --- | --- |
| Radar default boundary | All real backends and live-reader features default to `OFF` on every platform. A clean macOS ARM64 configuration confirmed the disabled cache values. |
| Radar build | The complete default CMake graph built successfully with tests and `strategy_lab` enabled. |
| Radar tests | CTest passed 42 of 42 tests. The strategy catalog completed 187 scenarios with 0 failures. The five tier demos built and entered simulation mode. |
| Vision environment | The frozen lock resolved 143 packages under Python 3.12.12 with the `dev` extra. CI and contributor commands used frozen resolution. |
| Vision lint and format | Ruff check and format verification passed across 333 source and test files. |
| Vision CPU tests | The documented selection passed 778 tests, skipped 1, and deselected 54 live, GPU, or CUDA cases. The focused optimizer suite passed 17 tests. |
| Vision strict typing | The matrix checked each interpreter as its actual Python version instead of forcing Python 3.11. Mypy reached project diagnostics and remained red with 451 errors in 96 files across 252 source files. |

<!-- markdownlint-enable MD013 -->

The Vision type failures are broad existing debt, not one missing package.
Most are GUI mixin host-contract errors, optional GPU or training import
boundaries, and ordinary narrowing or annotation defects. Blanket ignores were
not added. The concrete optimizer bug found during diagnosis was fixed by
normalizing supported string output paths before `Path` operations.

## Demo and GitHub Pages

No static HTML artifact or GitHub Pages workflow exists in this repository.
Radar is exercised through compiled simulation binaries, and Vision is a local
CLI, GUI, and media pipeline. A static page would be a new presentation
surface, not evidence for either runtime, so no Pages workflow was added.

## Remaining blockers and limitations

- Create and review a canonical first commit before any release or CI result
  can be tied to immutable source.
- Remediate the strict-mypy backlog in architecture-aware batches, including a
  typed host contract for GUI mixins and narrow contracts for optional
  platform, GPU, and training dependencies.
- Run the declared Python 3.11, 3.12, and 3.13 CI matrix against the same
  candidate after typing is green.
- Validate real Radar backends only on an authorized Windows lab with the
  required privileges, driver or firmware fixtures, and hardware. Default
  local evidence is simulation-only.
- Validate Vision live capture, overlay behavior, training, CUDA, and device
  performance on supported hardware. Those lanes were intentionally excluded
  from the CPU suite.
- Review licensing, third-party notices, security posture, packaging, and
  publication authority independently for each subtree.

Do not publish from this checkout.
