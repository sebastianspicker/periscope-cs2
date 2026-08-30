"""Verified model-asset operations exposed independently of CLI handlers."""

from cs2_vision_access.application.model_assets.downloads import (
    ModelDownloadError,
    download_verified_https,
    require_https_download_url,
    require_sha256,
    verify_sha256_file,
)
from cs2_vision_access.application.model_assets.manifest import (
    MAX_MODEL_BYTES,
    ModelManifest,
    ModelManifestError,
    create_manifest,
    verify_model,
)

__all__ = [
    "MAX_MODEL_BYTES",
    "ModelManifest",
    "ModelManifestError",
    "ModelDownloadError",
    "create_manifest",
    "download_verified_https",
    "require_https_download_url",
    "require_sha256",
    "verify_model",
    "verify_sha256_file",
]
