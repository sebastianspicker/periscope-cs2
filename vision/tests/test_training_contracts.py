"""Tests for training.contracts SSOT (classes, layouts, profiles, dataset yaml)."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.training.contracts import (
    PRODUCT_CLASSES,
    PROFILES,
    VOMBIT_CLASSES,
    VOMBIT_TO_PLAYER,
    Layout,
    TrainProfile,
    assert_label_class_ids_compatible,
    assert_label_class_ids_compatible_tree,
    normalize_class_names,
    resolve_profile,
    resolve_train_hyperparameters,
    write_dataset_yaml,
)


class TrainingContractsTests(unittest.TestCase):
    def test_product_and_vombit_class_maps(self) -> None:
        self.assertEqual(PRODUCT_CLASSES, {0: "player"})
        self.assertEqual(
            VOMBIT_CLASSES,
            {0: "ct", 1: "ct_head", 2: "t", 3: "t_head"},
        )

    def test_vombit_to_player_collapses_all_ids(self) -> None:
        self.assertEqual(VOMBIT_TO_PLAYER, {0: 0, 1: 0, 2: 0, 3: 0})
        for vombit_id in VOMBIT_CLASSES:
            self.assertEqual(VOMBIT_TO_PLAYER[vombit_id], 0)
            self.assertIn(VOMBIT_TO_PLAYER[vombit_id], PRODUCT_CLASSES)

    def test_profiles_exist(self) -> None:
        for key in ("smoke", "local", "cloud_t4"):
            self.assertIn(key, PROFILES)
            profile = PROFILES[key]
            self.assertIsInstance(profile, TrainProfile)
            self.assertEqual(profile.name, key)
            self.assertGreater(profile.epochs, 0)
            self.assertNotEqual(profile.batch, 0)
            self.assertGreaterEqual(profile.image_size, 320)
            self.assertTrue(profile.base_model.endswith(".pt"))

        self.assertEqual(PROFILES["smoke"].epochs, 1)
        self.assertEqual(PROFILES["smoke"].batch, 1)
        self.assertEqual(PROFILES["smoke"].image_size, 640)
        self.assertEqual(PROFILES["smoke"].base_model, "yolo26n-seg.pt")

        self.assertEqual(PROFILES["local"].epochs, 100)
        self.assertEqual(PROFILES["local"].batch, -1)
        self.assertEqual(PROFILES["local"].image_size, 640)
        self.assertEqual(PROFILES["local"].base_model, "yolo26n-seg.pt")

        cloud = PROFILES["cloud_t4"]
        self.assertEqual(cloud.epochs, 150)
        self.assertEqual(cloud.batch, 16)
        self.assertEqual(cloud.image_size, 416)
        self.assertEqual(cloud.base_model, "yolo11n-seg.pt")
        self.assertEqual(cloud.patience, 50)
        self.assertEqual(cloud.lr0, 0.001)

    def test_resolve_profile(self) -> None:
        cloud = resolve_profile("cloud_t4")
        self.assertIs(cloud, PROFILES["cloud_t4"])
        self.assertEqual(cloud.epochs, 150)

        local = resolve_profile("local")
        self.assertIs(local, PROFILES["local"])

        with self.assertRaises(ValueError) as ctx:
            resolve_profile("not_a_profile")
        message = str(ctx.exception)
        self.assertIn("unknown train profile", message)
        self.assertIn("cloud_t4", message)
        self.assertIn("local", message)
        self.assertIn("smoke", message)

    def test_resolve_train_hyperparameters_defaults_to_cloud_t4(self) -> None:
        resolved = resolve_train_hyperparameters()
        cloud = PROFILES["cloud_t4"]
        self.assertEqual(resolved["profile_name"], "cloud_t4")
        self.assertEqual(resolved["epochs"], cloud.epochs)
        self.assertEqual(resolved["batch"], cloud.batch)
        self.assertEqual(resolved["image_size"], cloud.image_size)
        self.assertEqual(resolved["base_model"], cloud.base_model)
        self.assertEqual(resolved["lr0"], cloud.lr0)
        self.assertEqual(resolved["patience"], cloud.patience)

    def test_resolve_train_hyperparameters_named_profile(self) -> None:
        resolved = resolve_train_hyperparameters(profile="local")
        local = PROFILES["local"]
        self.assertEqual(resolved["profile_name"], "local")
        self.assertEqual(resolved["epochs"], local.epochs)
        self.assertEqual(resolved["batch"], local.batch)
        self.assertEqual(resolved["image_size"], local.image_size)
        self.assertEqual(resolved["base_model"], local.base_model)

    def test_resolve_train_hyperparameters_overrides(self) -> None:
        resolved = resolve_train_hyperparameters(
            profile="smoke",
            epochs=42,
            batch=8,
            image_size=512,
            base_model="custom.pt",
            lr0=0.01,
            patience=3,
        )
        self.assertEqual(resolved["profile_name"], "smoke")
        self.assertEqual(resolved["epochs"], 42)
        self.assertEqual(resolved["batch"], 8)
        self.assertEqual(resolved["image_size"], 512)
        self.assertEqual(resolved["base_model"], "custom.pt")
        self.assertEqual(resolved["lr0"], 0.01)
        self.assertEqual(resolved["patience"], 3)

        # Partial override leaves remaining profile fields.
        partial = resolve_train_hyperparameters(profile="local", epochs=5)
        self.assertEqual(partial["epochs"], 5)
        self.assertEqual(partial["batch"], PROFILES["local"].batch)
        self.assertEqual(partial["image_size"], PROFILES["local"].image_size)

    def test_resolve_train_hyperparameters_unknown_profile(self) -> None:
        with self.assertRaises(ValueError) as ctx:
            resolve_train_hyperparameters(profile="missing")
        self.assertIn("unknown train profile", str(ctx.exception))

    def test_assert_label_class_ids_compatible_pass(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            labels = Path(tmp)
            (labels / "a.txt").write_text(
                "0 0.1 0.2 0.3 0.4\n0 0.5 0.5 0.6 0.6\n",
                encoding="utf-8",
            )
            (labels / "b.txt").write_text("0 0.2 0.2 0.3 0.3\n", encoding="utf-8")
            (labels / "empty.txt").write_text("", encoding="utf-8")
            warnings = assert_label_class_ids_compatible(labels, PRODUCT_CLASSES)
            self.assertEqual(len(warnings), 1)
            self.assertIn("empty", warnings[0])

    def test_assert_label_class_ids_compatible_fail(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            labels = Path(tmp)
            (labels / "bad.txt").write_text(
                "0 0.1 0.2 0.3 0.4\n2 0.5 0.5 0.6 0.6\n",
                encoding="utf-8",
            )
            with self.assertRaises(ValueError) as ctx:
                assert_label_class_ids_compatible(labels, PRODUCT_CLASSES)
            message = str(ctx.exception)
            self.assertIn("incompatible", message)
            self.assertIn("class id 2", message)

    def test_assert_label_class_ids_compatible_vombit_ok(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            labels = Path(tmp)
            (labels / "m.txt").write_text(
                "0 0.1 0.1 0.2 0.2\n3 0.3 0.3 0.4 0.4\n",
                encoding="utf-8",
            )
            warnings = assert_label_class_ids_compatible(labels, VOMBIT_CLASSES)
            self.assertEqual(warnings, [])

    def test_assert_label_class_ids_compatible_recursive(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            labels = Path(tmp)
            train = labels / "train"
            train.mkdir()
            (train / "a.txt").write_text(
                "0 0.1 0.2 0.3 0.4\n",
                encoding="utf-8",
            )
            # Non-recursive misses nested files → no error / empty
            warnings = assert_label_class_ids_compatible(labels, PRODUCT_CLASSES)
            self.assertEqual(warnings, [])
            # Recursive finds nested ok
            warnings = assert_label_class_ids_compatible_tree(labels, PRODUCT_CLASSES)
            self.assertEqual(warnings, [])
            (train / "bad.txt").write_text(
                "9 0.1 0.2 0.3 0.4\n",
                encoding="utf-8",
            )
            with self.assertRaises(ValueError) as ctx:
                assert_label_class_ids_compatible(labels, PRODUCT_CLASSES, recursive=True)
            self.assertIn("class id 9", str(ctx.exception))

    def test_normalize_class_names_defaults(self) -> None:
        self.assertEqual(normalize_class_names(None), PRODUCT_CLASSES)
        self.assertEqual(
            normalize_class_names(None, default="product"),
            PRODUCT_CLASSES,
        )
        self.assertEqual(
            normalize_class_names(None, default="vombit"),
            VOMBIT_CLASSES,
        )

    def test_normalize_class_names_str_keys(self) -> None:
        self.assertEqual(
            normalize_class_names({"0": "player", "1": "corpse"}),
            {0: "player", 1: "corpse"},
        )
        self.assertEqual(
            normalize_class_names({0: "ct", 2: "t"}),
            {0: "ct", 2: "t"},
        )

    def test_write_dataset_yaml_flat_bootstrap(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = write_dataset_yaml(
                root,
                layout=Layout.FLAT_BOOTSTRAP,
                classes=VOMBIT_CLASSES,
                portable_path=False,
            )
            self.assertEqual(path, root / "dataset.yaml")
            text = path.read_text(encoding="utf-8")
            self.assertIn("train: images\n", text)
            self.assertIn("val: images\n", text)
            self.assertIn("nc: 4\n", text)
            self.assertIn("0: ct", text)
            self.assertIn("3: t_head", text)
            # Absolute path by default
            self.assertIn(f"path: {root.resolve().as_posix()}", text)

    def test_write_dataset_yaml_session_split_portable(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = write_dataset_yaml(
                root,
                layout=Layout.SESSION_SPLIT,
                classes=PRODUCT_CLASSES,
                portable_path=True,
            )
            text = path.read_text(encoding="utf-8")
            self.assertEqual(path.name, "dataset.yaml")
            self.assertIn("path: .\n", text)
            self.assertIn("train: images/train\n", text)
            self.assertIn("val: images/val\n", text)
            self.assertIn("nc: 1\n", text)
            self.assertIn("0: player", text)

    def test_write_dataset_yaml_defaults_to_product_classes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = write_dataset_yaml(root)
            text = path.read_text(encoding="utf-8")
            self.assertIn("nc: 1\n", text)
            self.assertIn("0: player", text)
            self.assertIn("train: images\n", text)

    def test_write_dataset_yaml_rejects_unknown_layout(self) -> None:
        with tempfile.TemporaryDirectory() as tmp, self.assertRaises(ValueError):
            write_dataset_yaml(Path(tmp), layout="not_a_layout")

    def test_cloud_reexports_vombit_as_default_classes(self) -> None:
        from cs2_vision_access.training.cloud import DEFAULT_CLASSES

        self.assertEqual(DEFAULT_CLASSES, VOMBIT_CLASSES)

    def test_local_reexports_profile_defaults(self) -> None:
        from cs2_vision_access.training.local import (
            DEFAULT_BATCH,
            DEFAULT_EPOCHS,
            DEFAULT_IMAGE_SIZE,
            SMOKE_BATCH,
            SMOKE_EPOCHS,
        )

        self.assertEqual(DEFAULT_EPOCHS, PROFILES["local"].epochs)
        self.assertEqual(DEFAULT_BATCH, PROFILES["local"].batch)
        self.assertEqual(DEFAULT_IMAGE_SIZE, PROFILES["local"].image_size)
        self.assertEqual(SMOKE_EPOCHS, PROFILES["smoke"].epochs)
        self.assertEqual(SMOKE_BATCH, PROFILES["smoke"].batch)

    def test_bundle_default_names_match_vombit(self) -> None:
        from cs2_vision_access.training.bundle import DEFAULT_NAMES

        self.assertEqual(
            DEFAULT_NAMES,
            [VOMBIT_CLASSES[i] for i in sorted(VOMBIT_CLASSES)],
        )


if __name__ == "__main__":
    unittest.main()
