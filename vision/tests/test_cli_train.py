from __future__ import annotations

import unittest

from cs2_vision_access.cli import build_parser
from cs2_vision_access.training import (
    DEFAULT_PROJECT_DIRECTORY,
    DEFAULT_RUN_NAME,
    SMOKE_BATCH,
    SMOKE_EPOCHS,
    resolve_train_hyperparameters,
)


class CliTrainTests(unittest.TestCase):
    def test_train_defaults_use_artifacts_layout(self) -> None:
        parser = build_parser()
        arguments = parser.parse_args(
            [
                "train",
                "--dataset-yaml",
                "configs/cs2-players.yaml",
                "--dataset-root",
                "data/cs2_players",
                "--base-model-origin",
                "test-origin",
                "--exported-model-license",
                "test-license",
            ]
        )
        self.assertEqual(arguments.project_directory, DEFAULT_PROJECT_DIRECTORY)
        self.assertEqual(arguments.run_name, DEFAULT_RUN_NAME)
        self.assertFalse(arguments.smoke)
        self.assertIsNone(arguments.epochs)
        self.assertIsNone(arguments.batch)
        epochs, batch, _image_size = resolve_train_hyperparameters(
            smoke=arguments.smoke,
            epochs=arguments.epochs,
            batch=arguments.batch,
            image_size=arguments.image_size,
        )
        self.assertEqual(epochs, 100)
        self.assertEqual(batch, -1)

    def test_train_smoke_flag_resolves_few_epoch_defaults(self) -> None:
        parser = build_parser()
        arguments = parser.parse_args(
            [
                "train",
                "--dataset-yaml",
                "configs/cs2-players.yaml",
                "--dataset-root",
                "data/cs2_players",
                "--base-model-origin",
                "test-origin",
                "--exported-model-license",
                "test-license",
                "--smoke",
                "--device",
                "cpu",
            ]
        )
        self.assertTrue(arguments.smoke)
        epochs, batch, image_size = resolve_train_hyperparameters(
            smoke=arguments.smoke,
            epochs=arguments.epochs,
            batch=arguments.batch,
            image_size=arguments.image_size,
        )
        self.assertEqual(epochs, SMOKE_EPOCHS)
        self.assertEqual(batch, SMOKE_BATCH)
        self.assertEqual(image_size, 640)
        self.assertEqual(arguments.device, "cpu")
