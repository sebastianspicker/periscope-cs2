"""Pinned HTTPS artifact download service, independent of command handlers."""

from __future__ import annotations

import hashlib
import hmac
import os
import tempfile
import urllib.request
from pathlib import Path
from urllib.parse import urlparse

_DOWNLOAD_CHUNK_SIZE = 1024 * 1024
_MAX_MODEL_DOWNLOAD_BYTES = 2 * 1024 * 1024 * 1024
_DOWNLOAD_TIMEOUT_SECONDS = 30
_GITHUB_DOWNLOAD_HOSTS = frozenset(
    {"github.com", "objects.githubusercontent.com", "release-assets.githubusercontent.com"}
)
_HUGGINGFACE_DOWNLOAD_HOSTS = frozenset(
    {
        "huggingface.co",
        "cdn-lfs.huggingface.co",
        "cdn-lfs-us-1.hf.co",
        "cas-bridge.xethub.hf.co",
        "transfer.xethub.hf.co",
    }
)


class ModelDownloadError(RuntimeError):
    """A pinned artifact could not be fetched and verified safely."""


def require_https_download_url(url: str) -> str:
    """Validate an HTTPS URL with no embedded credentials."""
    try:
        parsed = urlparse(url)
        hostname = parsed.hostname
        _ = parsed.port
    except ValueError as error:
        raise ModelDownloadError(f"invalid HTTPS download URL: {url!r}") from error
    if (
        parsed.scheme != "https"
        or not hostname
        or parsed.username is not None
        or parsed.password is not None
    ):
        raise ModelDownloadError(f"invalid HTTPS download URL: {url!r}")
    return url


def require_sha256(expected_sha256: str) -> str:
    """Validate and normalize a mandatory SHA-256 registry pin."""
    normalized = expected_sha256.strip().lower()
    if len(normalized) != 64 or any(char not in "0123456789abcdef" for char in normalized):
        raise ModelDownloadError("model registry entry requires a valid SHA-256 pin")
    return normalized


def sha256_file(path: Path) -> str:
    """Compute a SHA-256 digest without loading a model into memory."""
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(_DOWNLOAD_CHUNK_SIZE):
            digest.update(chunk)
    return digest.hexdigest()


def verify_sha256_file(path: Path, expected_sha256: str) -> str:
    """Verify a local artifact against its required registry pin."""
    expected = require_sha256(expected_sha256)
    actual = sha256_file(path)
    if not hmac.compare_digest(actual, expected):
        raise ModelDownloadError(f"SHA-256 mismatch: expected {expected}, got {actual}")
    return actual


def _approved_download_hosts(origin_hostname: str) -> frozenset[str]:
    hostname = origin_hostname.lower()
    if hostname in _GITHUB_DOWNLOAD_HOSTS:
        return _GITHUB_DOWNLOAD_HOSTS
    if hostname in _HUGGINGFACE_DOWNLOAD_HOSTS:
        return _HUGGINGFACE_DOWNLOAD_HOSTS
    return frozenset({hostname})


class _ApprovedRedirectHandler(urllib.request.HTTPRedirectHandler):
    def __init__(self, allowed_hosts: frozenset[str]) -> None:
        super().__init__()
        self._allowed_hosts = allowed_hosts

    def redirect_request(self, req, fp, code, msg, headers, newurl):  # type: ignore[no-untyped-def]
        del fp, code, msg, headers
        require_https_download_url(newurl)
        hostname = urlparse(newurl).hostname
        if hostname is None or hostname.lower() not in self._allowed_hosts:
            raise ModelDownloadError(f"redirected model download to an unapproved host: {newurl!r}")
        return urllib.request.Request(
            newurl,
            headers=dict(req.headers),
            origin_req_host=req.origin_req_host,
            unverifiable=True,
            method=req.get_method(),
        )


def download_verified_https(url: str, dest: Path, *, expected_sha256: str) -> str:
    """Download to a temporary file and atomically promote only a matching pin."""
    download_url = require_https_download_url(url)
    expected = require_sha256(expected_sha256)
    origin_hostname = urlparse(download_url).hostname
    if origin_hostname is None:
        raise ModelDownloadError(f"invalid HTTPS download URL: {url!r}")
    allowed_hosts = _approved_download_hosts(origin_hostname)
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.is_symlink() or dest.parent.is_symlink():
        raise ModelDownloadError("model download destination must not use symlinks")
    temp_path: Path | None = None
    try:
        opener = urllib.request.build_opener(_ApprovedRedirectHandler(allowed_hosts))
        request = urllib.request.Request(download_url, headers={"User-Agent": "cs2-vision-access"})
        with opener.open(request, timeout=_DOWNLOAD_TIMEOUT_SECONDS) as response:
            final_url = require_https_download_url(response.geturl())
            final_hostname = urlparse(final_url).hostname
            if final_hostname is None or final_hostname.lower() not in allowed_hosts:
                raise ModelDownloadError(
                    f"redirected model download to an unapproved host: {final_url!r}"
                )
            content_length = response.headers.get("Content-Length")
            try:
                expected_length = None if content_length is None else int(content_length)
            except ValueError as error:
                raise ModelDownloadError(
                    "model download returned an invalid Content-Length"
                ) from error
            if (
                expected_length is not None
                and not 0 <= expected_length <= _MAX_MODEL_DOWNLOAD_BYTES
            ):
                raise ModelDownloadError("model download exceeds the size limit")
            digest = hashlib.sha256()
            written = 0
            with tempfile.NamedTemporaryFile(
                dir=dest.parent, prefix=f".{dest.name}.", suffix=".download", delete=False
            ) as temporary:
                temp_path = Path(temporary.name)
                while chunk := response.read(_DOWNLOAD_CHUNK_SIZE):
                    written += len(chunk)
                    if written > _MAX_MODEL_DOWNLOAD_BYTES:
                        raise ModelDownloadError("model download exceeds the size limit")
                    digest.update(chunk)
                    temporary.write(chunk)
            if expected_length is not None and written != expected_length:
                raise ModelDownloadError("model download was truncated")
            actual = digest.hexdigest()
            if not hmac.compare_digest(actual, expected):
                raise ModelDownloadError(f"SHA-256 mismatch: expected {expected}, got {actual}")
        os.replace(temp_path, dest)
        temp_path = None
        return actual
    finally:
        if temp_path is not None:
            temp_path.unlink(missing_ok=True)
