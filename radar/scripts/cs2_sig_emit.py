"""Emit and install CS2 offset snapshot headers."""
from __future__ import annotations

import shutil
from pathlib import Path
from typing import Any

from cs2_sig_common import (
    CRITICAL_GLOBALS,
    DEFAULT_URL_BASE,
    ENTITY_CONSTANTS,
    FIELD_MAP,
    GLOBAL_KEYS,
)

def emit_hpp(snap: dict) -> str:
    """
    Emit the C++ embedded snapshot header from a snapshot dict.

    Emits *every* FIELD_MAP field (core + extended: dormant, armor, spotted,
    observer, bomb) plus all GLOBAL_KEYS and ENTITY_CONSTANTS. Never drops
    extended symbols that build_snapshot required — write+install must not
    strip adapters/real/cs2/offsets_snapshot.hpp.
    """
    for key in ("globals", "fields", "constants"):
        if key not in snap or not isinstance(snap[key], dict):
            raise RuntimeError(f"snapshot missing '{key}' object for header emit")

    g = snap["globals"]
    f = snap["fields"]
    c = snap["constants"]
    ts = snap.get("generated_at_utc", "unknown")
    source = snap.get("source", DEFAULT_URL_BASE)

    def hx(v: Any) -> str:
        return f"0x{int(v):X}"

    # Prefer GLOBAL_KEYS order; also require criticals present.
    for name in GLOBAL_KEYS:
        if name not in g:
            raise RuntimeError(f"snapshot.globals missing {name}")
    for crit in CRITICAL_GLOBALS:
        if not g.get(crit):
            raise RuntimeError(f"snapshot.globals critical {crit} is zero/missing")

    # ALL FIELD_MAP keys (deduped, order-preserving) must be present and emitted.
    field_names: list[str] = []
    seen_fields: set[str] = set()
    for _cls, name in FIELD_MAP:
        if name in seen_fields:
            continue
        seen_fields.add(name)
        field_names.append(name)
        if name not in f:
            raise RuntimeError(f"snapshot.fields missing {name} (required by FIELD_MAP)")

    # Any extra keys already in the snapshot (forward-compat) are emitted after.
    extra_fields = sorted(k for k in f.keys() if k not in seen_fields)

    for name in ENTITY_CONSTANTS:
        if name not in c:
            raise RuntimeError(f"snapshot.constants missing {name}")

    lines: list[str] = [
        "#pragma once",
        "// Embedded CS2 offset snapshot (auto-updated by scripts/update_cs2_signatures.py).",
        "// Values are RVAs relative to client.dll base unless noted as field offsets.",
        f"// Source: {source}",
        f"// Generated: {ts}",
        "//",
        "// NOTE: entities.cpp / live_radar_stack.cpp require the full FIELD_MAP set",
        "// (core + armor, dormant, spotted, bomb, observer). emit_hpp emits every key.",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace real::cs2::snapshot {",
        "",
        "inline constexpr const char* kSourceUrl =",
        f'    "{source}";',
        'inline constexpr const char* kModule = "client.dll";',
        f'inline constexpr const char* kGeneratedAt = "{ts}";',
        "",
        "namespace globals {",
    ]
    for name in GLOBAL_KEYS:
        lines.append(f"inline constexpr std::uint64_t {name} = {hx(g[name])};")
    # Preserve any extra globals present in the snapshot.
    for name in sorted(k for k in g.keys() if k not in GLOBAL_KEYS):
        lines.append(f"inline constexpr std::uint64_t {name} = {hx(g[name])};")
    lines.append("}  // namespace globals")
    lines.append("")

    # Group field emits by schema class for readability (still every FIELD_MAP key).
    lines.append("namespace fields {")
    current_cls: str | None = None
    emitted: set[str] = set()
    for cls, name in FIELD_MAP:
        if name in emitted:
            continue
        if cls != current_cls:
            lines.append(f"// {cls}")
            current_cls = cls
        lines.append(f"inline constexpr std::uint64_t {name} = {hx(f[name])};")
        emitted.add(name)
    if extra_fields:
        lines.append("// additional snapshot fields")
        for name in extra_fields:
            lines.append(f"inline constexpr std::uint64_t {name} = {hx(f[name])};")
    lines.append("}  // namespace fields")
    lines.append("")

    lines.append("namespace constants {")
    # Fixed types: stride/table are uint64, mask/shift are uint32 (match prior ABI).
    lines.append(
        f"inline constexpr std::uint64_t entity_identity_stride = {hx(c['entity_identity_stride'])};"
    )
    lines.append(
        f"inline constexpr std::uint64_t entity_chunk_table_offset = {hx(c['entity_chunk_table_offset'])};"
    )
    lines.append(
        f"inline constexpr std::uint32_t entity_handle_index_mask = {hx(c['entity_handle_index_mask'])};"
    )
    lines.append(
        f"inline constexpr std::uint32_t entity_chunk_shift = {int(c['entity_chunk_shift'])}u;"
    )
    for name in sorted(k for k in c.keys() if k not in ENTITY_CONSTANTS):
        lines.append(f"inline constexpr std::uint64_t {name} = {hx(c[name])};")
    lines.append("}  // namespace constants")
    lines.append("")
    lines.append("}  // namespace real::cs2::snapshot")
    lines.append("")
    return "\n".join(lines)

def print_snapshot_summary(snap: dict) -> None:
    print("\nKey globals:")
    for k, v in snap["globals"].items():
        print(f"  {k} = 0x{int(v):X}")
    print("Key fields:")
    for k, v in snap["fields"].items():
        print(f"  {k} = 0x{int(v):X}")
    print("Constants:")
    for k, v in snap["constants"].items():
        if k == "entity_chunk_shift":
            print(f"  {k} = {int(v)}")
        else:
            print(f"  {k} = 0x{int(v):X}")


def print_changes(changes: list[str]) -> None:
    if changes == ["(no previous snapshot)"]:
        print("\nChange set: first snapshot")
    elif changes:
        print(f"\nDetected {len(changes)} offset change(s):")
        for line in changes:
            print(f"  {line}")
    else:
        print("\nNo offset value changes vs previous snapshot (timestamp will refresh).")


def install_header(hpp_text: str, out_dir: Path, radar_dir: Path) -> tuple[Path, Path]:
    """Write data/cs2 header and install into adapters/real/cs2/offsets_snapshot.hpp."""
    hpp_out = out_dir / "cs2_offsets_snapshot.hpp"
    try:
        hpp_out.write_text(hpp_text, encoding="utf-8")
    except OSError as exc:
        raise RuntimeError(f"cannot write {hpp_out}: {exc}") from exc

    cpp_header = radar_dir / "adapters" / "real" / "cs2" / "offsets_snapshot.hpp"
    try:
        cpp_header.parent.mkdir(parents=True, exist_ok=True)
        # Atomic-ish install: write via temp then replace when possible.
        tmp = cpp_header.with_suffix(".hpp.tmp")
        tmp.write_text(hpp_text, encoding="utf-8")
        tmp.replace(cpp_header)
    except OSError:
        # Fallback to copyfile if replace fails on some platforms.
        try:
            shutil.copyfile(hpp_out, cpp_header)
        except OSError as exc:
            raise RuntimeError(f"cannot install {cpp_header}: {exc}") from exc
    return hpp_out, cpp_header
