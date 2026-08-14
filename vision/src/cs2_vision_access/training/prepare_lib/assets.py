"""EdgeSAM/Vombit asset discovery, download, and path pickers."""

from __future__ import annotations

from pathlib import Path
from urllib.parse import unquote, urlparse

# Expected basenames (used in discovery error messages and preference order).
_DETECTOR_PREFERRED = (
    "yolov10n_cs2_fp16.onnx",
    "yolov10n_cs2.onnx",
    "yolov10s_cs2.onnx",
)
_ENCODER_PREFERRED = (
    "edge_sam_3x_encoder.onnx",
    "edge_sam_encoder.onnx",
)
_DECODER_PREFERRED = (
    "edge_sam_3x_decoder.onnx",
    "edge_sam_decoder.onnx",
)

# Registry keys downloaded by :func:`ensure_edgesam_assets`.
_EDGESAM_DOWNLOAD_KEYS = (
    "vombit-yolov10n-fp16",
    "edgesam-encoder",
    "edgesam-decoder",
)


def discover_edgesam_assets(artifacts_dir: Path | str) -> dict[str, Path]:
    """Find detector, manifest, encoder, decoder under *artifacts_dir*.

    Searches the directory itself and one level of subdirectories (non-recursive
    beyond that). Preference order matches README / download-model defaults:

    * detector: ``yolov10n_cs2_fp16.onnx``, then other ``*cs2*.onnx`` (not SAM)
    * manifest: ``{detector_stem}.model.json``, then ``*vombit*.model.json``
    * encoder: ``edge_sam*encoder*.onnx`` / ``*encoder*.onnx``
    * decoder: ``edge_sam*decoder*.onnx`` / ``*decoder*.onnx``

    Returns:
        Mapping with keys ``detector``, ``manifest``, ``encoder``, ``decoder``.

    Raises:
        FileNotFoundError: Directory missing or any required asset not found.
    """
    root = Path(artifacts_dir)
    if not root.is_dir():
        raise FileNotFoundError(
            f"artifacts directory not found: {root}\n"
            f"Expected files such as: {_DETECTOR_PREFERRED[0]}, "
            f"*.model.json, {_ENCODER_PREFERRED[0]}, {_DECODER_PREFERRED[0]}"
        )

    files = _collect_asset_files(root)
    detector = _pick_detector(files)
    encoder = _pick_encoder(files)
    decoder = _pick_decoder(files)
    manifest = _pick_manifest(files, detector) if detector is not None else None

    missing: list[str] = []
    if detector is None:
        missing.append(f"detector (e.g. {', '.join(_DETECTOR_PREFERRED)} or *cs2*.onnx)")
    if manifest is None:
        missing.append("manifest (*.model.json near detector, or *vombit*.model.json)")
    if encoder is None:
        missing.append(f"encoder (e.g. {', '.join(_ENCODER_PREFERRED)} or *encoder*.onnx)")
    if decoder is None:
        missing.append(f"decoder (e.g. {', '.join(_DECODER_PREFERRED)} or *decoder*.onnx)")
    if missing:
        found_names = sorted(p.name for p in files) if files else []
        found_msg = ", ".join(found_names) if found_names else "(none)"
        raise FileNotFoundError(
            f"could not discover EdgeSAM/Vombit assets under {root}: "
            f"missing {'; '.join(missing)}. Files seen: {found_msg}"
        )

    assert detector is not None and manifest is not None
    assert encoder is not None and decoder is not None
    return {
        "detector": detector,
        "manifest": manifest,
        "encoder": encoder,
        "decoder": decoder,
    }


def ensure_edgesam_assets(
    artifacts_dir: Path | str,
    *,
    download: bool = True,
) -> dict[str, Path]:
    """Discover EdgeSAM/Vombit assets or download the teacher pack.

    Tries :func:`discover_edgesam_assets` first. When assets are missing and
    *download* is True, downloads registry models ``vombit-yolov10n-fp16``,
    ``edgesam-encoder``, and ``edgesam-decoder`` into *artifacts_dir*, then
    re-discovers.

    Returns:
        Mapping with keys ``detector``, ``manifest``, ``encoder``, ``decoder``.

    Raises:
        FileNotFoundError: Assets still missing after optional download, or a
            download fails.
    """
    root = Path(artifacts_dir)
    try:
        return discover_edgesam_assets(root)
    except FileNotFoundError:
        if not download:
            raise

    root.mkdir(parents=True, exist_ok=True)
    try:
        _download_edgesam_registry_models(root)
    except Exception as exc:
        raise FileNotFoundError(
            f"failed to download EdgeSAM/Vombit assets into {root}: {exc}"
        ) from exc

    try:
        return discover_edgesam_assets(root)
    except FileNotFoundError as exc:
        raise FileNotFoundError(
            f"EdgeSAM assets still incomplete under {root} after download: {exc}"
        ) from exc


def _download_edgesam_registry_models(artifacts_dir: Path) -> None:
    """Fetch Vombit FP16 detector + EdgeSAM encoder/decoder via MODEL_REGISTRY."""
    import urllib.request

    from cs2_vision_access.config.data.model_registry import MODEL_REGISTRY
    from cs2_vision_access.model_manifest import create_manifest

    for key in _EDGESAM_DOWNLOAD_KEYS:
        info = MODEL_REGISTRY.get(key)
        if not isinstance(info, dict):
            raise FileNotFoundError(f"model registry missing entry: {key}")
        url = str(info.get("url") or "").strip()
        if not url:
            raise FileNotFoundError(f"no download URL for registry model {key}")

        basename = Path(unquote(urlparse(url).path)).name or f"{key}.onnx"
        if not basename.lower().endswith(".onnx"):
            basename = f"{key}.onnx"
        dest = artifacts_dir / basename
        if not dest.is_file():
            print(f"  Downloading {key}: {url} → {dest}")
            try:
                urllib.request.urlretrieve(url, dest)
            except Exception as exc:
                raise FileNotFoundError(f"download failed for {key} from {url}: {exc}") from exc
            print(f"  Saved {dest.name}")
        else:
            print(f"  {dest.name} already present (skip download)")

        # Detector needs a class manifest for Cs2SamSegmenter; EdgeSAM parts do not.
        classes_raw = info.get("classes")
        if not isinstance(classes_raw, dict) or not classes_raw:
            continue
        try:
            class_dict = {int(k): str(v) for k, v in classes_raw.items()}
        except (TypeError, ValueError) as exc:
            raise FileNotFoundError(
                f"invalid classes mapping for registry model {key}: {exc}"
            ) from exc
        license_name = str(info.get("license") or "unknown")
        for man_name in (f"{dest.stem}.model.json", f"{key}.model.json"):
            man_path = artifacts_dir / man_name
            if man_path.is_file():
                continue
            create_manifest(
                dest,
                man_path,
                classes=class_dict,
                origin=url,
                license_name=license_name,
                overwrite=False,
            )
            print(f"  Wrote manifest {man_path.name}")


def _collect_asset_files(root: Path) -> list[Path]:
    files: list[Path] = []
    try:
        entries = sorted(root.iterdir(), key=lambda p: p.name.lower())
    except OSError:
        return files
    for entry in entries:
        if entry.is_file():
            files.append(entry)
        elif entry.is_dir():
            try:
                for child in sorted(entry.iterdir(), key=lambda p: p.name.lower()):
                    if child.is_file():
                        files.append(child)
            except OSError:
                continue
    return files


def _pick_detector(files: list[Path]) -> Path | None:
    onnx = [f for f in files if f.suffix.lower() == ".onnx"]
    by_lower = {f.name.lower(): f for f in onnx}
    for name in _DETECTOR_PREFERRED:
        if name in by_lower:
            return by_lower[name]
    candidates = [
        f
        for f in onnx
        if "cs2" in f.name.lower()
        and "encoder" not in f.name.lower()
        and "decoder" not in f.name.lower()
    ]
    if candidates:
        return sorted(candidates, key=lambda p: p.name.lower())[0]
    return None


def _pick_encoder(files: list[Path]) -> Path | None:
    onnx = [f for f in files if f.suffix.lower() == ".onnx"]
    by_lower = {f.name.lower(): f for f in onnx}
    for name in _ENCODER_PREFERRED:
        if name in by_lower:
            return by_lower[name]
    preferred = [
        f
        for f in onnx
        if "encoder" in f.name.lower()
        and ("edge_sam" in f.name.lower() or "edgesam" in f.name.lower())
    ]
    if preferred:
        return sorted(preferred, key=lambda p: p.name.lower())[0]
    fallback = [f for f in onnx if "encoder" in f.name.lower()]
    if fallback:
        return sorted(fallback, key=lambda p: p.name.lower())[0]
    return None


def _pick_decoder(files: list[Path]) -> Path | None:
    onnx = [f for f in files if f.suffix.lower() == ".onnx"]
    by_lower = {f.name.lower(): f for f in onnx}
    for name in _DECODER_PREFERRED:
        if name in by_lower:
            return by_lower[name]
    preferred = [
        f
        for f in onnx
        if "decoder" in f.name.lower()
        and ("edge_sam" in f.name.lower() or "edgesam" in f.name.lower())
    ]
    if preferred:
        return sorted(preferred, key=lambda p: p.name.lower())[0]
    fallback = [f for f in onnx if "decoder" in f.name.lower()]
    if fallback:
        return sorted(fallback, key=lambda p: p.name.lower())[0]
    return None


def _pick_manifest(files: list[Path], detector: Path | None) -> Path | None:
    if detector is not None:
        sibling = detector.with_name(f"{detector.stem}.model.json")
        if sibling.is_file():
            return sibling
        for f in files:
            if f.name.lower() == f"{detector.stem.lower()}.model.json":
                return f
    model_jsons = [f for f in files if f.name.lower().endswith(".model.json")]
    if not model_jsons:
        return None
    vombit = [f for f in model_jsons if "vombit" in f.name.lower() or "cs2" in f.name.lower()]
    pool = vombit if vombit else model_jsons
    return sorted(pool, key=lambda p: p.name.lower())[0]
