#!/usr/bin/env python3
"""
update_cs2_signatures.py — Auto-update CS2 offsets/signatures from a2x/cs2-dumper.

Educational lab helper: pulls the latest public dump so entity-list / schema
field offsets stay current after game updates. Pure transform/diff logic is
separated from network/filesystem I/O and is covered by --self-test.

Usage:
  python scripts/update_cs2_signatures.py
  python scripts/update_cs2_signatures.py --out-dir data/cs2
  python scripts/update_cs2_signatures.py --check
  python scripts/update_cs2_signatures.py --skip-download
  python scripts/update_cs2_signatures.py --skip-download --check
  python scripts/update_cs2_signatures.py --self-test
  python scripts/update_cs2_signatures.py --url-base https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/

CMake:
  cmake --build build --target update_cs2_signatures

Writes (write mode):
  <out-dir>/offsets.json              (raw dumper file)
  <out-dir>/client_dll.json           (raw dumper file)
  <out-dir>/offsets_snapshot.json     (lab-trimmed snapshot; hot-loaded by demos)
  <out-dir>/cs2_offsets_snapshot.hpp  (C++ header, also copied to lib/real/cs2/)

Exit codes:
  0  success / check up-to-date / self-test pass
  1  hard failure (network, IO, missing critical data, self-test fail)
  2  --check: dump differs from installed snapshot
"""

from __future__ import annotations

import argparse
import json
import sys
import traceback
from pathlib import Path

# Allow `python scripts/update_cs2_signatures.py` to resolve sibling modules.
_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from cs2_sig_common import DEFAULT_URL_BASE
from cs2_sig_emit import emit_hpp, install_header, print_changes, print_snapshot_summary
from cs2_sig_fetch import (
    build_snapshot,
    diff_snapshots,
    fetch,
    load_json_bytes,
    load_json_path,
)
from cs2_sig_selftest import run_self_test


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Auto-update CS2 offsets/signatures from a2x/cs2-dumper",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--url-base",
        default=DEFAULT_URL_BASE,
        help="Base URL for dumper JSON files",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory (default: <repo>/code/data/cs2)",
    )
    parser.add_argument(
        "--skip-download",
        action="store_true",
        help="Reuse existing offsets.json / client_dll.json in out-dir",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Fetch/compare only; do not write (exit 2 if dump differs from snapshot)",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Run pure-logic self-tests against fixtures / on-disk dumps and exit",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=60.0,
        help="HTTP timeout seconds for downloads (default: 60)",
    )
    args = parser.parse_args(argv)

    script_dir = Path(__file__).resolve().parent
    code_dir = script_dir.parent

    if args.self_test:
        return run_self_test(script_dir, code_dir)

    out_dir = args.out_dir or (code_dir / "data" / "cs2")
    try:
        out_dir = out_dir.resolve()
    except OSError as exc:
        print(f"Invalid --out-dir {out_dir}: {exc}", file=sys.stderr)
        return 1

    if not args.check:
        try:
            out_dir.mkdir(parents=True, exist_ok=True)
        except OSError as exc:
            print(f"Cannot create out-dir {out_dir}: {exc}", file=sys.stderr)
            return 1

    offsets_path = out_dir / "offsets.json"
    client_path = out_dir / "client_dll.json"
    snap_path = out_dir / "offsets_snapshot.json"

    prev_snap: dict | None = None
    if snap_path.exists():
        try:
            prev_snap = load_json_path(snap_path)
        except RuntimeError as exc:
            print(f"WARNING: previous snapshot unreadable: {exc}", file=sys.stderr)
            prev_snap = None

    try:
        if not args.skip_download:
            base = args.url_base.rstrip("/")
            print(f"Fetching offsets from {args.url_base}")
            offsets_bytes = fetch(base + "/offsets.json", timeout=args.timeout)
            client_bytes = fetch(base + "/client_dll.json", timeout=args.timeout)
            offsets = load_json_bytes(offsets_bytes)
            client_dll = load_json_bytes(client_bytes)
            if not args.check:
                try:
                    offsets_path.write_bytes(offsets_bytes)
                    client_path.write_bytes(client_bytes)
                except OSError as exc:
                    print(f"Failed writing raw dumps: {exc}", file=sys.stderr)
                    return 1
                print(f"  wrote {offsets_path}")
                print(f"  wrote {client_path}")
        else:
            if not offsets_path.is_file() or not client_path.is_file():
                print(
                    "Missing offsets.json/client_dll.json for --skip-download "
                    f"(looked in {out_dir})",
                    file=sys.stderr,
                )
                return 1
            print(f"Reusing local dumps in {out_dir}")
            offsets = load_json_path(offsets_path)
            client_dll = load_json_path(client_path)

        snap = build_snapshot(offsets, client_dll, args.url_base)
    except (RuntimeError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:  # noqa: BLE001
        print(f"ERROR: unexpected failure: {exc}", file=sys.stderr)
        traceback.print_exc(file=sys.stderr)
        return 1

    changes = diff_snapshots(prev_snap, snap)
    print_snapshot_summary(snap)
    print_changes(changes)

    if args.check:
        if not prev_snap:
            print(
                "\n--check: no installed offsets_snapshot.json to compare against "
                "(treat as differs).",
                file=sys.stderr,
            )
            print("--check: exit 2 (no baseline / differs).")
            return 2
        if changes:
            print("\n--check: offsets differ from installed snapshot (exit 2).")
            return 2
        print("\n--check: snapshot is up to date.")
        return 0

    try:
        snap_path.write_text(json.dumps(snap, indent=2) + "\n", encoding="utf-8")
        print(f"\n  wrote {snap_path}")
        hpp = emit_hpp(snap)
        hpp_out, cpp_header = install_header(hpp, out_dir, code_dir)
        print(f"  wrote {hpp_out}")
        print(f"  installed {cpp_header}")
    except (RuntimeError, OSError) as exc:
        print(f"ERROR writing outputs: {exc}", file=sys.stderr)
        return 1

    print("\nDone. Rebuild live_radar / radar_t0 to pick up the embedded snapshot.")
    print("Runtime JSON is also hot-loaded from offsets_snapshot.json next to the exe.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
