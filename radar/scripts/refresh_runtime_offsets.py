#!/usr/bin/env python3
"""Refresh the runtime offsets snapshot safely (E58).

`update_cs2_signatures.py` is the repository-side generator: it rewrites the
pinned header and expects a rebuild. This script is the *runtime* channel: it
fetches the same public dump, verifies optional SHA-256 pins, builds the
trimmed snapshot and atomically replaces the file named by
`LR_CS2_OFFSETS_PATH`.

Usage:
    python radar/scripts/refresh_runtime_offsets.py --output %LOCALAPPDATA%/periscope/offsets_snapshot.json
    python radar/scripts/refresh_runtime_offsets.py --output ... --sha256 <hex> --sha256-client <hex>

The pins are the SHA-256 of the raw `offsets.json` and `client_dll.json`
responses. Without pins the content is still bounded and shape-checked by the
loader, but pinning turns a compromised CDN into a clean failure.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import urllib.request
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from cs2_sig_common import DEFAULT_URL_BASE  # noqa: E402
from cs2_sig_fetch import build_snapshot  # noqa: E402

MAX_BYTES = 4 * 1024 * 1024


def fetch(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "periscope/1.0"})
    with urllib.request.urlopen(request, timeout=20) as response:  # noqa: S310
        body = response.read(MAX_BYTES + 1)
    if len(body) > MAX_BYTES:
        raise ValueError(f"{url}: response over {MAX_BYTES} bytes")
    return body


def check_pin(body: bytes, expected: str | None, label: str) -> None:
    if not expected:
        return
    actual = hashlib.sha256(body).hexdigest()
    if actual.lower() != expected.lower():
        raise ValueError(f"{label} sha256 mismatch: {actual} != {expected}")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--url-base", default=DEFAULT_URL_BASE)
    parser.add_argument("--sha256", help="pin for raw offsets.json")
    parser.add_argument("--sha256-client", help="pin for raw client_dll.json")
    args = parser.parse_args(argv)

    base = args.url_base.rstrip("/")
    offsets_body = fetch(base + "/offsets.json")
    client_body = fetch(base + "/client_dll.json")
    check_pin(offsets_body, args.sha256, "offsets.json")
    check_pin(client_body, args.sha256_client, "client_dll.json")

    snapshot = build_snapshot(
        json.loads(offsets_body.decode("utf-8")),
        json.loads(client_body.decode("utf-8")),
        base + "/",
    )
    encoded = json.dumps(snapshot, indent=2, sort_keys=True).encode("utf-8")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_bytes(encoded)
    temporary.replace(args.output)
    print(f"wrote {args.output} ({len(encoded)} bytes)")
    print(f"set LR_CS2_OFFSETS_PATH={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
