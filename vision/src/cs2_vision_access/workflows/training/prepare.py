#!/usr/bin/env python3
"""Generate YOLO segmentation training data from CS2 gameplay video.

This script:
1. Takes a video file (recorded CS2 gameplay or from CS2-10k)
2. Runs the **cs2-sam** pipeline per sampled frame:
   - Vombit YOLOv10n+CS2 detector (team-aware bounding boxes)
   - EdgeSAM decoder (precise player-shaped masks per box)
3. Saves frames as JPEG images + YOLO-format .txt label files

Output structure::

    <output_dir>/
    ├── images/
    │   ├── frame_00000000.jpg
    │   ├── frame_00000001.jpg
    │   └── ...
    ├── labels/
    │   ├── frame_00000000.txt
    │   ├── frame_00000001.txt
    │   └── ...
    └── dataset.yaml          (Ultralytics config, auto-generated)

With ``--session-id <id>`` files land under ``<output_dir>/<id>/{images,labels}``.

Programmatic API: :func:`cs2_vision_access.workflows.training.prepare_lib.run_cs2_sam_prepare`.

Usage::

    # From a single video file
    uv run python -m cs2_vision_access.workflows.training.prepare ^
        --video my_gameplay.mp4 ^
        --detector artifacts/yolov10n_cs2_fp16.onnx ^
        --manifest artifacts/vombit-yolov10n-cs2.model.json ^
        --encoder artifacts/edge_sam_3x_encoder.onnx ^
        --decoder artifacts/edge_sam_3x_decoder.onnx ^
        --output data/cs2_train ^
        --sample-rate 3

    # Collapse Vombit classes to a single player id + keep empty negatives
    uv run python -m cs2_vision_access.workflows.training.prepare ^
        --video my_gameplay.mp4 ^
        --detector artifacts/yolov10n_cs2_fp16.onnx ^
        --manifest artifacts/vombit-yolov10n-cs2.model.json ^
        --encoder artifacts/edge_sam_3x_encoder.onnx ^
        --decoder artifacts/edge_sam_3x_decoder.onnx ^
        --output data/staging ^
        --session-id clip-001 ^
        --collapse-to-player ^
        --keep-negatives ^
        --device cuda
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Ensure the project src is on sys.path.
_PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from cs2_vision_access.application.ports.segmentation import Cs2SamSegmenter
from cs2_vision_access.workflows.training.contracts import VOMBIT_TO_PLAYER
from cs2_vision_access.workflows.training.prepare_lib import run_cs2_sam_prepare
from cs2_vision_access.workflows.training.sources import (
    _from_live_screen,
    _from_tar,
    _write_dataset_yaml,
    ensure_image_label_dirs,
    next_frame_index,
    resolve_write_root,
)


def parse_class_map_spec(spec: str) -> dict[int, int]:
    """Parse ``0=0,1=0,2=0,3=0`` style class id remaps."""
    mapping: dict[int, int] = {}
    for raw_part in spec.split(","):
        part = raw_part.strip()
        if not part:
            continue
        key_text, sep, value_text = part.partition("=")
        if not sep:
            raise argparse.ArgumentTypeError(f"invalid class-map entry {part!r}; expected SRC=DST")
        try:
            src = int(key_text.strip())
            dst = int(value_text.strip())
        except ValueError as exc:
            raise argparse.ArgumentTypeError(
                f"invalid class-map entry {part!r}; expected integer ids"
            ) from exc
        mapping[src] = dst
    if not mapping:
        raise argparse.ArgumentTypeError("class-map must contain at least one SRC=DST pair")
    return mapping


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Generate YOLO segmentation training data from CS2 gameplay",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    # Source (one of --video / --tar / --live-screen)
    src = p.add_argument_group("source (choose one)")
    src.add_argument("--video", help="Path to a .mp4 file of CS2 gameplay")
    src.add_argument("--tar", help="Path to a WebDataset .tar shard (CS2-10k)")
    src.add_argument("--live-screen", action="store_true", help="Capture live CS2 screen")

    # Model paths
    p.add_argument("--detector", required=True, help="Vombit YOLOv10 ONNX model")
    p.add_argument("--manifest", required=True, help="Detector manifest JSON")
    p.add_argument("--encoder", required=True, help="EdgeSAM encoder ONNX")
    p.add_argument("--decoder", required=True, help="EdgeSAM decoder ONNX")

    # Output
    p.add_argument("--output", default="data/cs2_train", help="Output directory")
    p.add_argument(
        "--session-id",
        default=None,
        help="Optional staging session id; write under output/<session_id>/{images,labels}",
    )
    p.add_argument(
        "--sample-rate",
        type=float,
        default=3.0,
        help="Frames per second to sample from video (default: 3)",
    )
    p.add_argument(
        "--confidence",
        type=float,
        default=0.4,
        help="Detection confidence threshold (default: 0.4)",
    )
    p.add_argument(
        "--min-confidence",
        type=float,
        default=None,
        help="Post-predict instance confidence floor (default: same as --confidence filter only)",
    )
    p.add_argument(
        "--min-mask-area",
        type=float,
        default=0.0,
        help="Drop instances whose polygon area (px) is below this (default: 0)",
    )
    p.add_argument(
        "--max-frames", type=int, default=0, help="Maximum frames to process (0 = unlimited)"
    )
    p.add_argument(
        "--max-players",
        type=int,
        default=0,
        help="Cap detections per frame (0 = unlimited, keeps highest-confidence)",
    )
    p.add_argument(
        "--device",
        default="cpu",
        help="ONNX Runtime device for detector/EdgeSAM (e.g. cpu, cuda, cuda:0)",
    )

    # Negatives / class map
    p.add_argument(
        "--keep-negatives",
        action="store_true",
        help="Also write frames with no detections as empty-label negatives",
    )
    p.add_argument(
        "--negative-every-n",
        type=int,
        default=None,
        help="When --keep-negatives, write every Nth empty attempt (default: 5)",
    )
    p.add_argument(
        "--class-map",
        type=str,
        default=None,
        help="Remap source class ids before write, e.g. 0=0,1=0,2=0,3=0",
    )
    p.add_argument(
        "--collapse-to-player",
        action="store_true",
        help="Shorthand for --class-map 0=0,1=0,2=0,3=0 (PRODUCT single-class yaml)",
    )

    return p


def _resolve_class_map(args: argparse.Namespace) -> dict[int, int] | None:
    if args.collapse_to_player and args.class_map:
        raise SystemExit("use only one of --collapse-to-player or --class-map")
    if args.collapse_to_player:
        return dict(VOMBIT_TO_PLAYER)
    if args.class_map:
        return parse_class_map_spec(args.class_map)
    return None


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> None:
    parser = _build_parser()
    args = parser.parse_args()

    # Validate source
    source_modes = sum([bool(args.video), bool(args.tar), args.live_screen])
    if source_modes != 1:
        parser.error("exactly one of --video, --tar, or --live-screen must be specified")

    class_map = _resolve_class_map(args)
    keep_negatives = bool(args.keep_negatives)
    if args.negative_every_n is not None:
        negative_every_n = args.negative_every_n
    else:
        negative_every_n = 5 if keep_negatives else 0

    # Video path: shared library API (keeps CLI and train-auto in sync).
    if args.video:
        # Library only supports collapse_to_player bool; custom map stays on CLI path.
        use_library_path = not (args.class_map and not args.collapse_to_player)
        if use_library_path:
            try:
                result = run_cs2_sam_prepare(
                    detector=args.detector,
                    manifest=args.manifest,
                    encoder=args.encoder,
                    decoder=args.decoder,
                    output_dir=args.output,
                    video=args.video,
                    session_id=args.session_id,
                    device=args.device,
                    confidence=args.confidence,
                    sample_rate=args.sample_rate,
                    max_frames=args.max_frames,
                    max_players=args.max_players,
                    collapse_to_player=bool(args.collapse_to_player),
                    keep_negatives=keep_negatives,
                    negative_every_n=negative_every_n,
                    min_mask_area=args.min_mask_area,
                    min_confidence=args.min_confidence,
                    write_yaml=True,
                )
            except FileNotFoundError as exc:
                raise SystemExit(str(exc)) from exc
            print(f"\nDone! {result.labeled_frames} labeled frames in {result.write_root}")
            print(
                f"Next: run 'uv run python -m cs2_vision_access.workflows.training.train "
                f"--data {result.write_root}/dataset.yaml'"
            )
            return
        # else: fall through to the CLI-specific video path which supports custom class maps.

    # Prepare output directory (flat or session staging layout) — tar / live / custom map
    output_dir = Path(args.output)
    write_root = resolve_write_root(output_dir, args.session_id)
    ensure_image_label_dirs(write_root)

    start_index = next_frame_index(write_root)
    if start_index > 0:
        print(f"Resuming: next frame index {start_index} in {write_root}")

    # Initialize cs2-sam segmenter
    print(f"Initializing cs2-sam segmenter (device={args.device})...")
    segmenter = Cs2SamSegmenter(
        args.detector,
        args.manifest,
        class_names=("ct", "ct_head", "t", "t_head"),
        confidence=args.confidence,
        image_size=640,
        device=args.device,
        sam_encoder_path=args.encoder,
        sam_decoder_path=args.decoder,
    )

    process_kw = dict(
        start_index=start_index,
        keep_negatives=keep_negatives,
        negative_every_n=negative_every_n,
        class_map=class_map,
        min_confidence=args.min_confidence,
        min_mask_area=args.min_mask_area,
    )

    if args.video:
        # Custom --class-map without --collapse-to-player.
        print(f"Processing video: {args.video}")
        from cs2_vision_access.workflows.training.sources import _from_video

        labeled = _from_video(
            args.video,
            segmenter,
            write_root,
            args.sample_rate,
            args.max_frames,
            args.max_players,
            **process_kw,
        )
    elif args.tar:
        print(f"Processing tar shard: {args.tar}")
        labeled = _from_tar(
            args.tar,
            segmenter,
            write_root,
            args.sample_rate,
            args.max_frames,
            args.max_players,
            **process_kw,
        )
    elif args.live_screen:
        labeled = _from_live_screen(
            segmenter,
            write_root,
            args.max_frames,
            args.max_players,
            **process_kw,
        )
    else:
        labeled = 0  # unreachable

    _write_dataset_yaml(write_root, class_map=class_map)
    print(f"\nDone! {labeled} labeled frames in {write_root}")
    print(
        f"Next: run 'uv run python -m cs2_vision_access.workflows.training.train "
        f"--data {write_root}/dataset.yaml'"
    )


if __name__ == "__main__":
    main()
