#!/usr/bin/env python3
"""Batch-process multiple CS2 gameplay sources into a single training dataset.

Processes multiple video files and/or CS2-10k tar shards sequentially,
accumulating labeled frames in the same output directory.

Usage::

    # Process multiple CS2-10k shards
    uv run python -m cs2_vision_access.training.batch ^
        --sources data/shards/ancient-000000.tar data/shards/mirage-000000.tar ^
        --detector artifacts/yolov10n_cs2_fp16.onnx ^
        --manifest artifacts/vombit-yolov10n-cs2.model.json ^
        --encoder artifacts/edge_sam_3x_encoder.onnx ^
        --decoder artifacts/edge_sam_3x_decoder.onnx ^
        --output data/cs2_train ^
        --sample-rate 2 ^
        --max-frames-per-source 300

    # Process multiple recorded gameplay videos
    uv run python -m cs2_vision_access.training.batch ^
        --sources gameclips/dust2.mp4 gameclips/mirage.mp4 gameclips/ancient.mp4 ^
        --detector artifacts/yolov10n_cs2_fp16.onnx ^
        --manifest artifacts/vombit-yolov10n-cs2.model.json ^
        --encoder artifacts/edge_sam_3x_encoder.onnx ^
        --decoder artifacts/edge_sam_3x_decoder.onnx ^
        --output data/cs2_train ^
        --sample-rate 3
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

_PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from cs2_vision_access.training.contracts import VOMBIT_TO_PLAYER
from cs2_vision_access.training.prepare import parse_class_map_spec


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Batch-process multiple CS2 gameplay sources",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "--sources", nargs="+", required=True, help="Video (.mp4) or tar shard files to process"
    )
    p.add_argument("--detector", required=True)
    p.add_argument("--manifest", required=True)
    p.add_argument("--encoder", required=True)
    p.add_argument("--decoder", required=True)
    p.add_argument("--output", default="data/cs2_train")
    p.add_argument(
        "--session-id",
        default=None,
        help="Optional staging session id; write under output/<session_id>/{images,labels}",
    )
    p.add_argument("--sample-rate", type=float, default=2.0)
    p.add_argument(
        "--max-frames-per-source",
        type=int,
        default=300,
        help="Max labeled frames per source (0 = unlimited)",
    )
    p.add_argument("--confidence", type=float, default=0.4)
    p.add_argument(
        "--min-confidence",
        type=float,
        default=None,
        help="Post-predict instance confidence floor",
    )
    p.add_argument(
        "--min-mask-area",
        type=float,
        default=0.0,
        help="Drop instances whose polygon area (px) is below this",
    )
    p.add_argument("--max-players", type=int, default=10)
    p.add_argument(
        "--device",
        default="cpu",
        help="ONNX Runtime device for detector/EdgeSAM (e.g. cpu, cuda, cuda:0)",
    )
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
        help="Shorthand for --class-map 0=0,1=0,2=0,3=0",
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


def main() -> None:
    parser = _build_parser()
    args = parser.parse_args()

    from cs2_vision_access.segmenters.cs2_sam import Cs2SamSegmenter
    from cs2_vision_access.training.sources import (
        _process_frame,
        _write_dataset_yaml,
        ensure_image_label_dirs,
        next_frame_index,
        resolve_write_root,
    )

    class_map = _resolve_class_map(args)
    keep_negatives = bool(args.keep_negatives)
    if args.negative_every_n is not None:
        negative_every_n = args.negative_every_n
    else:
        negative_every_n = 5 if keep_negatives else 0

    output_dir = Path(args.output)
    write_root = resolve_write_root(output_dir, args.session_id)
    ensure_image_label_dirs(write_root)

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

    total_labeled = 0
    frame_counter = next_frame_index(write_root)
    if frame_counter > 0:
        print(f"Resuming: next frame index {frame_counter} in {write_root}")

    negative_counter = [0]
    proc_kw = dict(
        keep_negatives=keep_negatives,
        negative_every_n=negative_every_n,
        negative_counter=negative_counter,
        class_map=class_map,
        min_confidence=args.min_confidence,
        min_mask_area=args.min_mask_area,
    )

    for source_path in args.sources:
        path = Path(source_path)
        if not path.is_file():
            print(f"WARNING: {path} not found, skipping")
            continue

        print(f"\nProcessing [{total_labeled + 1}/{len(args.sources)}]: {path.name}")

        import os
        import tempfile

        import cv2

        labeled_this = 0

        if path.suffix.lower() in (".mp4", ".avi", ".mov", ".mkv"):
            cap = cv2.VideoCapture(str(path))
            fps = cap.get(cv2.CAP_PROP_FPS)
            sample_interval = max(1, int(round(fps / args.sample_rate))) if fps > 0 else 16

            frame_idx = 0
            while True:
                ret, frame = cap.read()
                if not ret:
                    break
                if frame_idx % sample_interval == 0:
                    if _process_frame(
                        frame,
                        segmenter,
                        frame_counter,
                        write_root,
                        args.max_players,
                        **proc_kw,
                    ):
                        labeled_this += 1
                        frame_counter += 1
                    if (
                        args.max_frames_per_source > 0
                        and labeled_this >= args.max_frames_per_source
                    ):
                        break
                frame_idx += 1

            cap.release()

        elif path.suffix.lower() == ".tar":
            try:
                import webdataset as wds
            except ImportError:
                print("webdataset package required for .tar files; pip install webdataset")
                continue

            dataset = wds.WebDataset(str(path)).decode()
            for sample in dataset:
                video_data = sample.get("mp4")
                if video_data is None:
                    continue

                with tempfile.NamedTemporaryFile(suffix=".mp4", delete=False) as f:
                    f.write(video_data)
                    tmp_path = f.name

                cap = cv2.VideoCapture(tmp_path)
                fps = cap.get(cv2.CAP_PROP_FPS)
                sample_interval = max(1, int(round(fps / args.sample_rate))) if fps > 0 else 48

                vid_idx = 0
                while True:
                    ret, frame = cap.read()
                    if not ret:
                        break
                    if vid_idx % sample_interval == 0:
                        if _process_frame(
                            frame,
                            segmenter,
                            frame_counter,
                            write_root,
                            args.max_players,
                            **proc_kw,
                        ):
                            labeled_this += 1
                            frame_counter += 1
                        if (
                            args.max_frames_per_source > 0
                            and labeled_this >= args.max_frames_per_source
                        ):
                            break
                    vid_idx += 1

                cap.release()
                os.unlink(tmp_path)

                if args.max_frames_per_source > 0 and labeled_this >= args.max_frames_per_source:
                    break
        else:
            print(f"WARNING: unknown file type {path.suffix}, skipping")
            continue

        print(f"  → {labeled_this} labeled frames from {path.name}")
        total_labeled += labeled_this

    _write_dataset_yaml(write_root, class_map=class_map)
    print(f"\n{'=' * 60}")
    print(f"Batch processing complete! {total_labeled} total labeled frames")
    print(f"Output: {write_root}")
    print(
        f"Next: uv run python -m cs2_vision_access.training.train --data {write_root}/dataset.yaml"
    )
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
