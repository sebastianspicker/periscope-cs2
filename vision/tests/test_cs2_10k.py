"""Tests for CS2-10k WebDataset materialization helpers."""

from __future__ import annotations

import inspect
import io
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import cv2
import numpy as np

from cs2_vision_access.training.cs2_10k import (
    CS2_10K_MAPS,
    extract_frames_from_tar,
    materialize_cs2_10k_dataset,
)


def _make_tiny_mp4(path: Path, frames: int = 8, size: int = 32) -> None:
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(str(path), fourcc, 8.0, (size, size))
    assert writer.isOpened(), "VideoWriter failed (codec)"
    for i in range(frames):
        img = np.full((size, size, 3), i * 20 % 255, dtype=np.uint8)
        writer.write(img)
    writer.release()


def _make_webdataset_tar(tar_path: Path, n_videos: int = 2) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        with tarfile.open(tar_path, "w") as tf:
            for i in range(n_videos):
                mp4 = tmp_path / f"clip-{i}.mp4"
                _make_tiny_mp4(mp4, frames=16)
                tf.add(mp4, arcname=f"{i:04d}.mp4")
                # parquet sibling (ignored by extractor)
                info = tarfile.TarInfo(name=f"{i:04d}.parquet")
                payload = b"PAR1fake"
                info.size = len(payload)
                tf.addfile(info, io.BytesIO(payload))


class ExtractFramesFromTarTests(unittest.TestCase):
    def test_extracts_sampled_frames(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tar_path = root / "shard.tar"
            out = root / "out"
            _make_webdataset_tar(tar_path, n_videos=2)
            videos, frames = extract_frames_from_tar(
                tar_path,
                out,
                max_videos=2,
                frames_per_video=3,
                sample_every_n=2,
                shuffle_videos=False,
                random_offset=False,
            )
            self.assertEqual(videos, 2)
            self.assertGreaterEqual(frames, 2)
            imgs = list((out / "images").glob("frame_*.jpg"))
            self.assertEqual(len(imgs), frames)

    def test_shuffle_is_deterministic(self) -> None:
        """Same seed yields identical frame outputs; different seed can differ."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tar_path = root / "shard.tar"
            _make_webdataset_tar(tar_path, n_videos=4)

            def run(seed: int, out_name: str) -> list[str]:
                out = root / out_name
                extract_frames_from_tar(
                    tar_path,
                    out,
                    max_videos=2,
                    frames_per_video=2,
                    sample_every_n=3,
                    shuffle_videos=True,
                    seed=seed,
                    random_offset=True,
                )
                return sorted(p.name for p in (out / "images").glob("frame_*.jpg"))

            names_a = run(123, "out_a")
            names_b = run(123, "out_b")
            names_c = run(999, "out_c")
            self.assertEqual(names_a, names_b)
            self.assertTrue(names_a)  # extracted something
            # Pixel content of first frame should match for same seed.
            img_a = cv2.imread(str(root / "out_a" / "images" / names_a[0]))
            img_b = cv2.imread(str(root / "out_b" / "images" / names_b[0]))
            self.assertIsNotNone(img_a)
            self.assertIsNotNone(img_b)
            self.assertTrue(np.array_equal(img_a, img_b))
            # Different seed: at least the signature exists; content may differ.
            self.assertTrue(names_c)
            img_c = cv2.imread(str(root / "out_c" / "images" / names_c[0]))
            self.assertIsNotNone(img_c)

    def test_reproducible_rng_keeps_same_seed_sampling_contract(self) -> None:
        """The non-cryptographic RNG is deliberate so training inputs reproduce."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tar_path = root / "shard.tar"
            _make_webdataset_tar(tar_path, n_videos=4)
            outputs: list[list[str]] = []
            for name in ("first", "second"):
                out = root / name
                extract_frames_from_tar(
                    tar_path,
                    out,
                    max_videos=2,
                    frames_per_video=2,
                    sample_every_n=3,
                    shuffle_videos=True,
                    seed=2026,
                    random_offset=True,
                )
                outputs.append(sorted(path.name for path in (out / "images").glob("frame_*.jpg")))
            self.assertEqual(outputs[0], outputs[1])

    def test_random_offset_parameter_exists(self) -> None:
        sig = inspect.signature(extract_frames_from_tar)
        self.assertIn("random_offset", sig.parameters)
        self.assertIn("shuffle_videos", sig.parameters)
        self.assertIn("seed", sig.parameters)
        self.assertTrue(sig.parameters["random_offset"].default is True)
        self.assertTrue(sig.parameters["shuffle_videos"].default is True)

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tar_path = root / "shard.tar"
            out = root / "out"
            _make_webdataset_tar(tar_path, n_videos=1)
            videos, frames = extract_frames_from_tar(
                tar_path,
                out,
                max_videos=1,
                frames_per_video=2,
                sample_every_n=2,
                random_offset=True,
                seed=7,
            )
            self.assertEqual(videos, 1)
            self.assertGreaterEqual(frames, 1)


class MaterializeCs210kTests(unittest.TestCase):
    def test_materialize_uses_downloaded_shards(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tar_path = root / "mirage-000000.tar"
            _make_webdataset_tar(tar_path, n_videos=1)
            out = root / "dataset"

            with (
                patch(
                    "cs2_vision_access.training.cs2_10k.download_shards",
                    return_value=[tar_path],
                ),
            ):
                report = materialize_cs2_10k_dataset(
                    out,
                    maps=("mirage",),
                    max_shards=1,
                    max_videos=1,
                    frames_per_video=4,
                    sample_every_n=2,
                    cache_dir=root / "cache",
                    shuffle_videos=True,
                    seed=42,
                    random_offset=True,
                )
            self.assertGreater(report.frames_written, 0)
            self.assertTrue((out / "images").is_dir())
            self.assertIn("mirage", report.maps)


class MapConstantsTests(unittest.TestCase):
    def test_maps_include_common_cs2_maps(self) -> None:
        for name in ("mirage", "dust2", "inferno", "ancient"):
            self.assertIn(name, CS2_10K_MAPS)


class HoldoutVideoFractionTests(unittest.TestCase):
    def test_holdout_writes_images_val_and_json(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tar_path = root / "shard.tar"
            out = root / "out"
            _make_webdataset_tar(tar_path, n_videos=5)
            videos, frames = extract_frames_from_tar(
                tar_path,
                out,
                max_videos=5,
                frames_per_video=2,
                sample_every_n=2,
                shuffle_videos=True,
                seed=42,
                random_offset=False,
                holdout_video_fraction=0.4,
            )
            self.assertEqual(videos, 5)
            self.assertGreater(frames, 0)
            train_imgs = list((out / "images").glob("frame_*.jpg"))
            val_imgs = list((out / "images_val").glob("frame_*.jpg"))
            self.assertTrue(train_imgs, "expected non-holdout frames under images/")
            self.assertTrue(val_imgs, "expected holdout frames under images_val/")
            holdout_json = out / "cs2_10k_holdout_videos.json"
            self.assertTrue(holdout_json.is_file())
            payload = __import__("json").loads(holdout_json.read_text(encoding="utf-8"))
            self.assertIn("holdout_frames", payload)
            self.assertIn("holdout_videos", payload)
            self.assertEqual(
                set(payload["holdout_frames"]),
                {p.stem for p in val_imgs},
            )
            # Holdout stems must not appear under train images/
            self.assertFalse({p.stem for p in val_imgs} & {p.stem for p in train_imgs})

    def test_zero_holdout_fraction_skips_images_val(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tar_path = root / "shard.tar"
            out = root / "out"
            _make_webdataset_tar(tar_path, n_videos=3)
            extract_frames_from_tar(
                tar_path,
                out,
                max_videos=3,
                frames_per_video=2,
                sample_every_n=2,
                shuffle_videos=False,
                random_offset=False,
                holdout_video_fraction=0.0,
            )
            self.assertTrue(list((out / "images").glob("frame_*.jpg")))
            self.assertFalse(
                (out / "images_val").is_dir() and list((out / "images_val").glob("frame_*.jpg"))
            )


if __name__ == "__main__":
    unittest.main()
