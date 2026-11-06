#!/usr/bin/env python3
"""Convert GFusion-style `.opt` collision maps to Periscope `.tri` files (E46).

The `.opt` layout is the one produced by the donor's `VPhysToOpt` tool:

    u64 mesh_count
    repeated mesh_count times:
        u64 triangle_count
        repeated triangle_count times: 9 x f32 (v0.xyz, v1.xyz, v2.xyz)

The `.tri` layout is documented in
`radar/src/application/radar/tri_loader.hpp`:

    "TRIZ" | u32 version (1) | u32 triangle_count | 36 * N bytes

The converter is deliberately strict: counts must fit the remaining file and
coordinates must be finite. A truncated file fails rather than producing a
partial map that silently loses walls.

Usage:
    python radar/scripts/opt_to_tri.py --input-dir <dir> --output-dir radar/data/maps
    python radar/scripts/opt_to_tri.py --input map.opt --output map.tri
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

U64 = struct.Struct("<Q")
F32X9 = struct.Struct("<9f")
TRI_HEADER = struct.Struct("<4sII")


def convert(source: Path) -> bytes:
    raw = source.read_bytes()
    offset = 0

    if len(raw) < U64.size:
        raise ValueError("file is shorter than the mesh count")
    (mesh_count,) = U64.unpack_from(raw, offset)
    offset += U64.size
    if mesh_count > 1_000_000:
        raise ValueError(f"implausible mesh count {mesh_count}")

    triangles: list[tuple[float, ...]] = []
    for mesh_index in range(mesh_count):
        if offset + U64.size > len(raw):
            raise ValueError(f"truncated mesh header at mesh {mesh_index}")
        (triangle_count,) = U64.unpack_from(raw, offset)
        offset += U64.size
        if triangle_count > 10_000_000:
            raise ValueError(f"implausible triangle count {triangle_count}")
        need = triangle_count * F32X9.size
        if offset + need > len(raw):
            raise ValueError(f"truncated triangles at mesh {mesh_index}")
        for triangle_index in range(triangle_count):
            values = F32X9.unpack_from(raw, offset)
            offset += F32X9.size
            if not all(math.isfinite(value) for value in values):
                raise ValueError(
                    f"non-finite coordinate at mesh {mesh_index} "
                    f"triangle {triangle_index}"
                )
            triangles.append(values)

    if offset != len(raw):
        raise ValueError(f"{len(raw) - offset} trailing byte(s)")

    if len(triangles) > 0xFFFFFFFF:
        raise ValueError("too many triangles for the v1 format")

    out = bytearray(TRI_HEADER.pack(b"TRIZ", 1, len(triangles)))
    for values in triangles:
        out += F32X9.pack(*values)
    return bytes(out)


def convert_one(source: Path, output: Path) -> None:
    data = convert(source)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_bytes(data)
    temporary.replace(output)
    print(f"{source.name}: {len(data)} bytes -> {output}")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, help="one .opt file")
    parser.add_argument("--output", type=Path, help="output .tri path")
    parser.add_argument("--input-dir", type=Path, help="directory of .opt files")
    parser.add_argument("--output-dir", type=Path, help="directory for .tri files")
    args = parser.parse_args(argv)

    if args.input and args.output:
        convert_one(args.input, args.output)
        return 0
    if args.input_dir and args.output_dir:
        sources = sorted(args.input_dir.glob("*.opt"))
        if not sources:
            print(f"no .opt files in {args.input_dir}", file=sys.stderr)
            return 1
        for source in sources:
            convert_one(source, args.output_dir / (source.stem + ".tri"))
        return 0

    parser.print_help()
    return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
