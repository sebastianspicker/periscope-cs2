"""Shared model operations — ONNX export, manifest creation, hashing, COCO classes.

These were extracted from ``download_model.py`` so that ``setup.py`` (and
other callers) can import them without depending on the download CLI handler.
"""

from __future__ import annotations

import hashlib
import hmac
import os
import tempfile
import urllib.request
from pathlib import Path
from typing import Final
from urllib.parse import urlparse

from cs2_vision_access.application.model_assets.manifest import (
    create_manifest as write_model_manifest,
)
from cs2_vision_access.application.model_assets.manifest import (
    sha256_file as _canonical_sha256_file,
)


class ModelOperationError(RuntimeError):
    """Model export or manifest creation failed."""


_DOWNLOAD_CHUNK_SIZE: Final = 1024 * 1024
_MAX_MODEL_DOWNLOAD_BYTES: Final = 2 * 1024 * 1024 * 1024
_DOWNLOAD_TIMEOUT_SECONDS: Final = 30
_GITHUB_DOWNLOAD_HOSTS: Final = frozenset(
    {"github.com", "objects.githubusercontent.com", "release-assets.githubusercontent.com"}
)
_HUGGINGFACE_DOWNLOAD_HOSTS: Final = frozenset(
    {
        "huggingface.co",
        "cdn-lfs.huggingface.co",
        "cdn-lfs-us-1.hf.co",
        "cas-bridge.xethub.hf.co",
        "transfer.xethub.hf.co",
    }
)


def require_https_download_url(url: str) -> str:
    """Validate an HTTPS URL suitable for a pinned model artifact."""
    try:
        parsed = urlparse(url)
        hostname = parsed.hostname
        _ = parsed.port
    except ValueError as error:
        raise ModelOperationError(f"invalid HTTPS download URL: {url!r}") from error
    if (
        parsed.scheme != "https"
        or not hostname
        or parsed.username is not None
        or parsed.password is not None
    ):
        raise ModelOperationError(f"invalid HTTPS download URL: {url!r}")
    return url


def require_sha256(expected_sha256: str) -> str:
    """Validate and normalize a required SHA-256 registry pin."""
    normalized = expected_sha256.strip().lower()
    if len(normalized) != 64 or any(char not in "0123456789abcdef" for char in normalized):
        raise ModelOperationError("model registry entry requires a valid SHA-256 pin")
    return normalized


def verify_sha256_file(path: Path, expected_sha256: str) -> str:
    """Verify a model artifact against a required registry SHA-256 pin."""
    expected = require_sha256(expected_sha256)
    actual = sha256_file(path)
    if not hmac.compare_digest(actual, expected):
        raise ModelOperationError(f"SHA-256 mismatch: expected {expected}, got {actual}")
    return actual


def _approved_download_hosts(origin_hostname: str) -> frozenset[str]:
    """Return the small redirect allowlist for a registry artifact origin."""
    hostname = origin_hostname.lower()
    if hostname in _GITHUB_DOWNLOAD_HOSTS:
        return _GITHUB_DOWNLOAD_HOSTS
    if hostname in _HUGGINGFACE_DOWNLOAD_HOSTS:
        return _HUGGINGFACE_DOWNLOAD_HOSTS
    return frozenset({hostname})


class _ApprovedRedirectHandler(urllib.request.HTTPRedirectHandler):
    """Permit only HTTPS redirects to the registry origin's approved hosts."""

    def __init__(self, allowed_hosts: frozenset[str]) -> None:
        super().__init__()
        self._allowed_hosts = allowed_hosts

    def redirect_request(self, req, fp, code, msg, headers, newurl):  # type: ignore[no-untyped-def]
        del fp, code, msg, headers
        require_https_download_url(newurl)
        hostname = urlparse(newurl).hostname
        if hostname is None or hostname.lower() not in self._allowed_hosts:
            raise ModelOperationError(
                f"redirected model download to an unapproved host: {newurl!r}"
            )
        return urllib.request.Request(
            newurl,
            headers=dict(req.headers),
            origin_req_host=req.origin_req_host,
            unverifiable=True,
            method=req.get_method(),
        )


def download_verified_https(url: str, dest: Path, *, expected_sha256: str) -> str:
    """Stream a pinned HTTPS artifact to a temporary file and atomically promote it.

    Redirects are limited to the source host or the explicitly supported CDN
    hosts for GitHub and Hugging Face. A destination is never created or
    replaced until its streamed SHA-256 matches the registry pin.
    """
    download_url = require_https_download_url(url)
    expected = require_sha256(expected_sha256)
    origin_hostname = urlparse(download_url).hostname
    if origin_hostname is None:  # Covered above; keeps the type invariant explicit.
        raise ModelOperationError(f"invalid HTTPS download URL: {url!r}")
    allowed_hosts = _approved_download_hosts(origin_hostname)
    dest.parent.mkdir(parents=True, exist_ok=True)
    temp_path: Path | None = None
    try:
        opener = urllib.request.build_opener(_ApprovedRedirectHandler(allowed_hosts))
        request = urllib.request.Request(download_url, headers={"User-Agent": "cs2-vision-access"})
        with opener.open(request, timeout=_DOWNLOAD_TIMEOUT_SECONDS) as response:
            final_url = require_https_download_url(response.geturl())
            final_hostname = urlparse(final_url).hostname
            if final_hostname is None or final_hostname.lower() not in allowed_hosts:
                raise ModelOperationError(
                    f"redirected model download to an unapproved host: {final_url!r}"
                )
            content_length = response.headers.get("Content-Length")
            if content_length is not None:
                try:
                    expected_length = int(content_length)
                except ValueError as error:
                    raise ModelOperationError(
                        "model download returned an invalid Content-Length"
                    ) from error
                if expected_length < 0 or expected_length > _MAX_MODEL_DOWNLOAD_BYTES:
                    raise ModelOperationError("model download exceeds the size limit")
            else:
                expected_length = None
            digest = hashlib.sha256()
            written = 0
            with tempfile.NamedTemporaryFile(
                dir=dest.parent,
                prefix=f".{dest.name}.",
                suffix=".download",
                delete=False,
            ) as temporary:
                temp_path = Path(temporary.name)
                while chunk := response.read(_DOWNLOAD_CHUNK_SIZE):
                    written += len(chunk)
                    if written > _MAX_MODEL_DOWNLOAD_BYTES:
                        raise ModelOperationError("model download exceeds the size limit")
                    digest.update(chunk)
                    temporary.write(chunk)
            if expected_length is not None and written != expected_length:
                raise ModelOperationError("model download was truncated")
            actual = digest.hexdigest()
            if not hmac.compare_digest(actual, expected):
                raise ModelOperationError(f"SHA-256 mismatch: expected {expected}, got {actual}")
        os.replace(temp_path, dest)
        temp_path = None
        return actual
    finally:
        if temp_path is not None:
            temp_path.unlink(missing_ok=True)


def export_onnx(pt_path: Path, onnx_path: Path, *, image_size: int, device: str) -> None:
    """Load a .pt checkpoint and export to ONNX."""
    try:
        from ultralytics import YOLO
    except ImportError as error:
        raise ModelOperationError(
            "Ultralytics is required for ONNX export; install project dependencies"
        ) from error

    try:
        model = YOLO(str(pt_path))
        exported_path = model.export(
            format="onnx",
            imgsz=image_size,
            dynamic=False,
            simplify=False,
            device=device,
        )
        exported = Path(exported_path)
        if exported.resolve() != onnx_path.resolve():
            import shutil

            shutil.move(str(exported), str(onnx_path))
    except Exception as error:
        raise ModelOperationError(f"ONNX export failed for {pt_path.name}: {error}") from error


def create_manifest(
    onnx_path: Path,
    manifest_path: Path,
    *,
    model_name: str,
    model_info: dict[str, str],
    image_size: int,
    origin: str,
    license_name: str,
    classes: list[str] | None,
) -> None:
    """Write a SHA-256 manifest JSON for the exported ONNX model.

    CLI-friendly wrapper around
    :func:`cs2_vision_access.application.model_assets.manifest.create_manifest`. Preserves origin
    enrichment (``download-model {model_name}``) used by download-model / setup.
    ``model_info`` and ``image_size`` are accepted for call-site compatibility.
    """
    del model_info, image_size  # API compatibility only

    if classes:
        class_dict = {i: name for i, name in enumerate(classes)}
    else:
        class_dict = {int(key): name for key, name in coco80_classes().items()}

    write_model_manifest(
        onnx_path,
        manifest_path,
        classes=class_dict,
        origin=f"{origin}; download-model {model_name}",
        license_name=license_name,
        overwrite=True,
    )


def sha256_file(path: Path) -> str:
    """Compute the SHA-256 hex digest of a file."""
    return _canonical_sha256_file(path)


def coco80_classes() -> dict[str, str]:
    """Return the standard COCO 80-class mapping as a str→str dict.

    Source of truth: ``cs2_vision_access.application.configuration.data.coco80.json``.
    """
    from cs2_vision_access.application.configuration.data import coco80_classes as _load

    return dict(_load())
