"""Unit tests for training.prepare_lib (EdgeSAM asset discovery + prepare API)."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

import numpy as np

from cs2_vision_access.training.bootstrap_labels import bootstrap_class_id
from cs2_vision_access.training.prepare_lib import (
    EdgesamBootstrapResult,
    PrepareResult,
    bootstrap_with_edgesam,
    candidate_image_dirs_for_labeling,
    discover_edgesam_assets,
    ensure_edgesam_assets,
    run_cs2_sam_prepare,
)


def _touch(path: Path, content: bytes = b"x") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return path


def _seed_assets(root: Path) -> dict[str, Path]:
    return {
        "detector": _touch(root / "yolov10n_cs2_fp16.onnx"),
        "manifest": _touch(root / "yolov10n_cs2_fp16.model.json"),
        "encoder": _touch(root / "edge_sam_3x_encoder.onnx"),
        "decoder": _touch(root / "edge_sam_3x_decoder.onnx"),
    }


class DiscoverEdgesamAssetsTests(unittest.TestCase):
    def test_raises_when_dir_missing(self) -> None:
        with self.assertRaises(FileNotFoundError):
            discover_edgesam_assets(Path("no/such/artifacts"))

    def test_discovers_preferred_names(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            assets = _seed_assets(root)
            found = discover_edgesam_assets(root)
            self.assertEqual(found["detector"], assets["detector"])
            self.assertEqual(found["encoder"], assets["encoder"])
            self.assertEqual(found["decoder"], assets["decoder"])
            self.assertTrue(found["manifest"].is_file())

    def test_partial_assets_raises(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _touch(root / "edge_sam_3x_encoder.onnx")
            _touch(root / "edge_sam_3x_decoder.onnx")
            with self.assertRaises(FileNotFoundError):
                discover_edgesam_assets(root)


class RunCs2SamPrepareTests(unittest.TestCase):
    def test_requires_exactly_one_source(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            assets = _seed_assets(root)
            with self.assertRaises(ValueError):
                run_cs2_sam_prepare(
                    **assets,
                    output_dir=root / "out",
                )
            with self.assertRaises(ValueError):
                run_cs2_sam_prepare(
                    **assets,
                    output_dir=root / "out",
                    video=root / "a.mp4",
                    images_dir=root / "images",
                )

    def test_images_dir_labels_in_place(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            assets = _seed_assets(root)
            data = root / "data"
            images = data / "images"
            labels = data / "labels"
            images.mkdir(parents=True)
            labels.mkdir(parents=True)
            (images / "f0.jpg").write_bytes(b"not-an-image")
            (images / "f1.jpg").write_bytes(b"not-an-image")

            mock_seg = MagicMock()
            mock_seg.predict.return_value = ()

            with (
                patch(
                    "cs2_vision_access.training.prepare_lib.prepare_run.Cs2SamSegmenter",
                    return_value=mock_seg,
                ),
                patch(
                    "cs2_vision_access.training.prepare_lib.prepare_run.cv2.imread",
                    return_value=np.zeros((8, 8, 3), dtype=np.uint8),
                ),
            ):
                result = run_cs2_sam_prepare(
                    **assets,
                    output_dir=data,
                    images_dir=images,
                    labels_dir=labels,
                    keep_negatives=True,
                    negative_every_n=1,
                    write_yaml=False,
                )

            self.assertIsInstance(result, PrepareResult)
            self.assertEqual(result.labeled_frames, 2)
            self.assertTrue((labels / "f0.txt").is_file())
            self.assertTrue((labels / "f1.txt").is_file())

    def test_session_split_images_resolves_labels_train(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            assets = _seed_assets(root)
            data = root / "split"
            images = data / "images" / "train"
            labels = data / "labels" / "train"
            images.mkdir(parents=True)
            labels.mkdir(parents=True)
            (images / "t0.jpg").write_bytes(b"x")

            mock_seg = MagicMock()
            mock_seg.predict.return_value = ()

            with (
                patch(
                    "cs2_vision_access.training.prepare_lib.prepare_run.Cs2SamSegmenter",
                    return_value=mock_seg,
                ),
                patch(
                    "cs2_vision_access.training.prepare_lib.prepare_run.cv2.imread",
                    return_value=np.zeros((8, 8, 3), dtype=np.uint8),
                ),
            ):
                result = run_cs2_sam_prepare(
                    **assets,
                    output_dir=data,
                    images_dir=images,
                    keep_negatives=True,
                    negative_every_n=1,
                    write_yaml=False,
                )

            self.assertEqual(result.labeled_frames, 1)
            self.assertTrue((labels / "t0.txt").is_file())


class EnsureEdgesamAssetsTests(unittest.TestCase):
    def test_returns_discovered_without_download(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            assets = _seed_assets(root)
            found = ensure_edgesam_assets(root, download=False)
            self.assertEqual(found["detector"], assets["detector"])

    def test_download_false_raises_when_missing(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            root.mkdir(exist_ok=True)
            with self.assertRaises(FileNotFoundError):
                ensure_edgesam_assets(root, download=False)

    def test_download_true_fetches_registry_models(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            def fake_urlretrieve(url, dest):
                Path(dest).write_bytes(b"onnx-bytes")

            with patch(
                "urllib.request.urlretrieve",
                side_effect=fake_urlretrieve,
            ):
                found = ensure_edgesam_assets(root, download=True)

            self.assertTrue(found["detector"].is_file())
            self.assertTrue(found["encoder"].is_file())
            self.assertTrue(found["decoder"].is_file())
            self.assertTrue(found["manifest"].is_file())


class BootstrapClassIdTests(unittest.TestCase):
    def test_prefers_zero(self) -> None:
        self.assertEqual(bootstrap_class_id({0: "player", 1: "other"}), 0)

    def test_first_sorted_key_when_no_zero(self) -> None:
        self.assertEqual(bootstrap_class_id({2: "a", 1: "b"}), 1)

    def test_empty_defaults_zero(self) -> None:
        self.assertEqual(bootstrap_class_id({}), 0)


class CandidateImageDirsTests(unittest.TestCase):
    def test_primary_and_images_val(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            images_val = root / "images_val"
            images.mkdir()
            images_val.mkdir()
            (images / "a.jpg").write_bytes(b"x")
            (images_val / "b.jpg").write_bytes(b"x")
            dirs = candidate_image_dirs_for_labeling(root)
            self.assertEqual(len(dirs), 2)
            self.assertEqual(dirs[0], images)
            self.assertEqual(dirs[1], images_val)

    def test_skips_empty_and_missing_val(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            images.mkdir()
            (images / "a.jpg").write_bytes(b"x")
            dirs = candidate_image_dirs_for_labeling(root)
            self.assertEqual(dirs, [images])

    def test_include_images_val_false(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            images_val = root / "images_val"
            images.mkdir()
            images_val.mkdir()
            (images / "a.jpg").write_bytes(b"x")
            (images_val / "b.jpg").write_bytes(b"x")
            dirs = candidate_image_dirs_for_labeling(root, include_images_val=False)
            self.assertEqual(dirs, [images])

    def test_custom_images_dir(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            custom = root / "frames"
            custom.mkdir()
            (custom / "c.jpg").write_bytes(b"x")
            dirs = candidate_image_dirs_for_labeling(root, images_dir=custom)
            self.assertEqual(dirs, [custom])


class BootstrapWithEdgesamTests(unittest.TestCase):
    def test_soft_fail_missing_assets(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "images" / "f0.jpg").write_bytes(b"x")
            art = root / "empty"
            art.mkdir()
            result = bootstrap_with_edgesam(root, artifacts_dir=art, download=False)
            self.assertIsInstance(result, EdgesamBootstrapResult)
            self.assertFalse(result.ok)
            self.assertEqual(result.labeled_frames, 0)
            self.assertIsNone(result.assets)
            self.assertTrue(any("assets" in n.lower() for n in result.notes))

    def test_mocked_prepare_labels_image_dirs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            assets = _seed_assets(root / "art")
            images = root / "images"
            images_val = root / "images_val"
            images.mkdir()
            images_val.mkdir()
            (images / "a.jpg").write_bytes(b"x")
            (images_val / "b.jpg").write_bytes(b"x")

            calls: list[dict] = []

            def fake_prepare(**kwargs):
                calls.append(dict(kwargs))
                return PrepareResult(labeled_frames=1, write_root=str(root), notes=[])

            with patch(
                "cs2_vision_access.training.prepare_lib.bootstrap.run_cs2_sam_prepare",
                side_effect=fake_prepare,
            ):
                result = bootstrap_with_edgesam(
                    root,
                    artifacts_dir=root / "art",
                    download=False,
                    device="cpu",
                    confidence=0.3,
                )

            self.assertTrue(result.ok)
            self.assertEqual(result.labeled_frames, 2)
            self.assertEqual(len(calls), 2)
            self.assertEqual(Path(str(calls[0]["images_dir"])), images)
            self.assertEqual(Path(str(calls[1]["images_dir"])), images_val)
            self.assertEqual(result.assets["detector"], assets["detector"])

    def test_explicit_assets_skip_discover(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            assets = _seed_assets(root / "elsewhere")
            images = root / "images"
            images.mkdir()
            (images / "a.jpg").write_bytes(b"x")

            with patch(
                "cs2_vision_access.training.prepare_lib.bootstrap.run_cs2_sam_prepare",
                return_value=PrepareResult(labeled_frames=3, write_root=str(root), notes=[]),
            ) as prepare:
                result = bootstrap_with_edgesam(
                    root,
                    explicit_assets=assets,
                    download=False,
                )

            self.assertTrue(result.ok)
            self.assertEqual(result.labeled_frames, 3)
            prepare.assert_called_once()
            self.assertEqual(
                Path(str(prepare.call_args.kwargs["detector"])),
                assets["detector"],
            )

    def test_prepare_error_returns_not_ok(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _seed_assets(root / "art")
            images = root / "images"
            images.mkdir()
            (images / "a.jpg").write_bytes(b"x")

            with patch(
                "cs2_vision_access.training.prepare_lib.bootstrap.run_cs2_sam_prepare",
                side_effect=RuntimeError("boom"),
            ):
                result = bootstrap_with_edgesam(root, artifacts_dir=root / "art", download=False)

            self.assertFalse(result.ok)
            self.assertTrue(any("prepare failed" in n for n in result.notes))


if __name__ == "__main__":
    unittest.main()
