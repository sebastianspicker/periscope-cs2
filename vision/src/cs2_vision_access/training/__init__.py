"""CS2 model training pipeline — local and cloud."""

from cs2_vision_access.training.local import (
    DEFAULT_BATCH,
    DEFAULT_EPOCHS,
    DEFAULT_IMAGE_SIZE,
    DEFAULT_PROJECT_DIRECTORY,
    DEFAULT_RUN_NAME,
    SMOKE_BATCH,
    SMOKE_EPOCHS,
    TrainingError,
    TrainingSummary,
    _normalise_class_names,
    _normalise_dataset_yaml_contract,
    resolve_train_hyperparameters,
    train_and_export,
)

__all__ = [
    "DEFAULT_BATCH",
    "DEFAULT_EPOCHS",
    "DEFAULT_IMAGE_SIZE",
    "DEFAULT_PROJECT_DIRECTORY",
    "DEFAULT_RUN_NAME",
    "SMOKE_BATCH",
    "SMOKE_EPOCHS",
    "TrainingError",
    "TrainingSummary",
    "_normalise_class_names",
    "_normalise_dataset_yaml_contract",
    "resolve_train_hyperparameters",
    "train_and_export",
]
