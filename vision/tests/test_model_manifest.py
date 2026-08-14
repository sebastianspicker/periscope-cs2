from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.model_manifest import (
    ModelManifest,
    ModelManifestError,
    create_manifest,
    verify_model,
)


class ModelManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.model = self.root / "player.onnx"
        self.model.write_bytes(b"deterministic fake ONNX fixture")
        self.manifest = self.root / "player.model.json"

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def register(self) -> None:
        create_manifest(
            self.model,
            self.manifest,
            classes={0: "player"},
            origin="unit-test",
            license_name="test-only",
        )

    def test_created_manifest_round_trips_and_verifies(self) -> None:
        self.register()

        model, manifest = verify_model(self.model, self.manifest)

        self.assertEqual(model, self.model.resolve())
        self.assertEqual(manifest.classes, {0: "player"})
        self.assertEqual(manifest.origin, "unit-test")

    def test_hash_mismatch_fails_closed(self) -> None:
        self.register()
        self.model.write_bytes(b"changed after registration")

        with self.assertRaisesRegex(ModelManifestError, "SHA-256 mismatch"):
            verify_model(self.model, self.manifest)

    def test_unknown_manifest_keys_are_rejected(self) -> None:
        self.register()
        payload = json.loads(self.manifest.read_text(encoding="utf-8"))
        payload["download_url"] = "https://invalid.example/model"
        self.manifest.write_text(json.dumps(payload), encoding="utf-8")

        with self.assertRaisesRegex(ModelManifestError, "unknown"):
            ModelManifest.load(self.manifest)

    def test_non_contiguous_class_ids_are_rejected(self) -> None:
        with self.assertRaisesRegex(ModelManifestError, "contiguous"):
            create_manifest(
                self.model,
                self.manifest,
                classes={1: "player"},
                origin="unit-test",
                license_name="test-only",
            )

    def test_runtime_rejects_pytorch_checkpoint_format(self) -> None:
        checkpoint = self.root / "player.pt"
        checkpoint.write_bytes(b"not a trusted format")

        with self.assertRaisesRegex(ModelManifestError, ".onnx"):
            create_manifest(
                checkpoint,
                self.manifest,
                classes={0: "player"},
                origin="unit-test",
                license_name="test-only",
            )

    def test_model_symlink_is_rejected(self) -> None:
        target = self.root / "target.onnx"
        target.write_bytes(b"target")
        link = self.root / "link.onnx"
        try:
            os.symlink(target, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable in this environment")

        with self.assertRaisesRegex(ModelManifestError, "symlink"):
            create_manifest(
                link,
                self.manifest,
                classes={0: "player"},
                origin="unit-test",
                license_name="test-only",
            )

    def test_manifest_cannot_replace_model(self) -> None:
        with self.assertRaisesRegex(ModelManifestError, "differ"):
            create_manifest(
                self.model,
                self.model,
                classes={0: "player"},
                origin="unit-test",
                license_name="test-only",
                overwrite=True,
            )


if __name__ == "__main__":
    unittest.main()
