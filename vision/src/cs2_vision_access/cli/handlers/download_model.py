"""download-model subcommand — download real ONNX weights and create manifests.

Downloads a named model from either:

* ``_DOWNLOADABLE_MODELS`` — Ultralytics ``.pt`` checkpoints, exported to ONNX
* ``MODEL_REGISTRY`` — recommended teachers (Vombit, EdgeSAM, …); ONNX-direct
  entries download the ``.onnx`` file and skip Ultralytics export

Then writes a SHA-256 checksum manifest when class metadata is available.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlparse

from cs2_vision_access.cli.handlers._model_ops import (
    coco80_classes,
    create_manifest,
    download_verified_https,
    export_onnx,
    require_https_download_url,
    require_sha256,
    sha256_file,
    verify_sha256_file,
)
from cs2_vision_access.config.data.model_registry import MODEL_REGISTRY

# Back-compat aliases used by tests and external callers.
_coco80_classes = coco80_classes
_sha256_file = sha256_file
_create_manifest = create_manifest


_DOWNLOADABLE_MODELS: dict[str, dict[str, str]] = {
    "yolo26n-seg": {
        "description": "YOLO26 Nano segmentation (2.7M params, 640px)",
        "pt_url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo26n-seg.pt",
        "default_origin": "https://docs.ultralytics.com/tasks/segment",
        "task": "segment",
    },
    "yolo26s-seg": {
        "description": "YOLO26 Small segmentation",
        "pt_url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo26s-seg.pt",
        "default_origin": "https://docs.ultralytics.com/tasks/segment",
        "task": "segment",
    },
    "yolo26m-seg": {
        "description": "YOLO26 Medium segmentation",
        "pt_url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo26m-seg.pt",
        "default_origin": "https://docs.ultralytics.com/tasks/segment",
        "task": "segment",
    },
    "yolo11n-seg": {
        "description": "YOLO11 Nano segmentation",
        "pt_url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11n-seg.pt",
        "default_origin": "https://docs.ultralytics.com/tasks/segment",
        "task": "segment",
    },
    "yolo11n": {
        "description": "YOLO11 Nano detection (box-only, for ultralytics-detect backend)",
        "pt_url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11n.pt",
        "default_origin": "https://docs.ultralytics.com/tasks/detect",
        "task": "detect",
    },
}

# Optional short aliases → registry / downloadable key.
# Full registry names (vombit-yolov10n-fp16, edgesam-encoder, edgesam-decoder)
# resolve directly via MODEL_REGISTRY.
_MODEL_ALIASES: dict[str, str] = {
    "vombit": "vombit-yolov10n",
    "vombit-fp16": "vombit-yolov10n-fp16",
}


class DownloadModelError(RuntimeError):
    """Model download or ONNX export failed."""


def _require_https_download_url(url: str) -> str:
    """Return a safe HTTPS download URL or raise ``DownloadModelError``."""
    try:
        return require_https_download_url(url)
    except Exception as error:
        raise DownloadModelError(str(error)) from error


def _required_registry_sha256(model_name: str, info: dict[str, Any]) -> str:
    """Return a registry pin or fail closed before any model bytes are loaded."""
    try:
        return require_sha256(str(info.get("sha256") or ""))
    except Exception as error:
        raise DownloadModelError(
            f"model {model_name!r} has no valid registry SHA-256 pin"
        ) from error


def _download_https(url: str, dest: Path, *, expected_sha256: str) -> str:
    """Download, verify, and atomically promote a pinned HTTPS model artifact."""
    try:
        download_url = require_https_download_url(url)
        expected = require_sha256(expected_sha256)
        return download_verified_https(download_url, dest, expected_sha256=expected)
    except Exception as error:
        raise DownloadModelError(str(error)) from error


def _verify_or_remove(path: Path, *, expected_sha256: str) -> str:
    """Reject and remove a mismatched artifact before it can be loaded or exported."""
    try:
        return verify_sha256_file(path, expected_sha256)
    except Exception as error:
        path.unlink(missing_ok=True)
        raise DownloadModelError(str(error)) from error


def _available_model_names() -> list[str]:
    """Sorted union of Ultralytics downloadables and registry teachers."""
    names = set(_DOWNLOADABLE_MODELS) | set(MODEL_REGISTRY) | set(_MODEL_ALIASES)
    return sorted(names)


def _resolve_model_name(model_name: str) -> str:
    """Resolve aliases; strip optional ``.pt`` / ``.onnx`` suffix."""
    name = model_name.strip()
    if name.endswith(".pt"):
        name = name[:-3]
    elif name.endswith(".onnx"):
        name = name[:-5]
    return _MODEL_ALIASES.get(name, name)


def _is_onnx_direct(info: dict[str, Any]) -> bool:
    """True when the entry points at a ready-made ONNX file (no Ultralytics export)."""
    url = str(info.get("url") or info.get("pt_url") or "")
    fmt = str(info.get("format") or "").lower()
    return fmt == "onnx" or url.lower().endswith(".onnx")


def _url_basename(url: str) -> str:
    path = unquote(urlparse(url).path)
    name = Path(path).name
    return name or "model.onnx"


def _registry_classes_as_list(info: dict[str, Any]) -> list[str] | None:
    """Return ordered class names from a registry entry, or None if absent."""
    raw = info.get("classes")
    if not isinstance(raw, dict) or not raw:
        return None
    try:
        items = sorted(((int(k), str(v)) for k, v in raw.items()), key=lambda item: item[0])
    except (TypeError, ValueError):
        return None
    if not items:
        return None
    return [name for _, name in items]


def register_download_model_command(subcommands: argparse._SubParsersAction[Any]) -> None:
    available = ", ".join(_available_model_names())
    dl = subcommands.add_parser(
        "download-model",
        help="download, export to ONNX, and create a checksum manifest for a real model",
        description=(
            "Downloads a named model: Ultralytics checkpoints are exported to ONNX; "
            "registry teachers (Vombit, EdgeSAM, …) with ONNX URLs are fetched directly. "
            "Writes a SHA-256 manifest when class metadata is available."
        ),
    )
    dl.add_argument(
        "model_name",
        nargs="?",
        default="yolo26n-seg",
        help=(f"Model name to download (default: yolo26n-seg). Available: {available}"),
    )
    dl.add_argument(
        "--output-dir",
        type=Path,
        default=Path("artifacts"),
        help="Output directory for ONNX and manifest (default: artifacts/)",
    )
    dl.add_argument(
        "--image-size",
        type=int,
        default=640,
        help="ONNX export image size in pixels (default: 640; ignored for ONNX-direct)",
    )
    dl.add_argument(
        "--device",
        default="cpu",
        help="Device for export (default: cpu; use cuda:0 for GPU; ignored for ONNX-direct)",
    )
    dl.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing ONNX and manifest files",
    )
    dl.add_argument(
        "--list-models",
        action="store_true",
        help="List available downloadable models and exit",
    )
    dl.add_argument(
        "--classes",
        nargs="*",
        default=None,
        metavar="NAME",
        help=(
            "Class names for the manifest (default: COCO 80 for Ultralytics YOLO-seg, "
            "or registry classes for teachers). Pass multiple names for multi-class."
        ),
    )
    dl.add_argument(
        "--origin",
        default=None,
        help="Override the default origin URL in the manifest",
    )
    dl.add_argument(
        "--license",
        default=None,
        help="License identifier for the manifest (default: model-specific or AGPL-3.0-only)",
    )
    dl.set_defaults(handler=_handle_download_model)


def _list_models() -> int:
    print("Available downloadable models:\n")
    print("  --- Ultralytics (.pt → ONNX export) ---")
    for name, info in sorted(_DOWNLOADABLE_MODELS.items()):
        print(f"  {name}")
        print(f"    {info['description']}")
        print(f"    task: {info['task']}")
        print(f"    url: {info['pt_url']}")
        print()

    print("  --- Registry teachers (ONNX-direct when format=onnx) ---")
    for name, info in sorted(MODEL_REGISTRY.items()):
        # Prefer listing under downloadables when both define the same name.
        if name in _DOWNLOADABLE_MODELS:
            continue
        kind = "onnx-direct" if _is_onnx_direct(info) else str(info.get("format", "?"))
        print(f"  {name}")
        print(f"    {info.get('description', '')}")
        print(f"    task: {info.get('task', '?')}")
        print(f"    format: {kind}")
        print(f"    url: {info.get('url', info.get('pt_url', ''))}")
        if info.get("recommended"):
            print("    recommended: yes")
        print()
    return 0


def _handle_download_model(arguments: argparse.Namespace) -> int:
    if arguments.list_models:
        return _list_models()

    model_name = _resolve_model_name(arguments.model_name)

    # Prefer a checkpoint only when it has its own pin. If the same name also
    # has a pinned registry artifact, use that safe path instead.
    downloadable_info = _DOWNLOADABLE_MODELS.get(model_name)
    if downloadable_info is not None and downloadable_info.get("sha256"):
        return _handle_pt_download(model_name, arguments)

    if model_name in MODEL_REGISTRY:
        registry_info = MODEL_REGISTRY[model_name]
        if _is_onnx_direct(registry_info):
            return _handle_onnx_direct(model_name, registry_info, arguments)
        # Non-ONNX registry entry with a pt-like URL: treat like downloadable.
        if registry_info.get("pt_url") or str(registry_info.get("url", "")).endswith(".pt"):
            return _handle_registry_pt(model_name, registry_info, arguments)
        print(f"error: registry model {model_name!r} is not ONNX-direct and has no .pt URL")
        return 2

    if downloadable_info is not None:
        return _handle_pt_download(model_name, arguments)

    available = ", ".join(_available_model_names())
    print(f"error: unknown model {model_name!r}. Available: {available}")
    return 2


def _handle_pt_download(model_name: str, arguments: argparse.Namespace) -> int:
    model_info = _DOWNLOADABLE_MODELS[model_name]
    expected_sha = _required_registry_sha256(model_name, model_info)
    output_dir = Path(arguments.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    pt_path = output_dir / f"{model_name}.pt"
    onnx_path = output_dir / f"{model_name}.onnx"
    manifest_path = output_dir / f"{model_name}.model.json"

    if pt_path.exists() and not arguments.overwrite:
        _verify_or_remove(pt_path, expected_sha256=expected_sha)
        print(f"  {pt_path.name} already exists (use --overwrite to replace)")
    else:
        print(f"  Downloading {model_info['pt_url']} ...")
        try:
            _download_https(model_info["pt_url"], pt_path, expected_sha256=expected_sha)
        except Exception as error:
            raise DownloadModelError(f"download failed for {model_name}: {error}") from error
        print(f"  Saved {pt_path}")

    if onnx_path.exists() and not arguments.overwrite:
        print(f"  {onnx_path.name} already exists (use --overwrite to replace)")
    else:
        print(f"  Exporting {pt_path.name} to ONNX (imgsz={arguments.image_size}) ...")
        _export_onnx(
            pt_path,
            onnx_path,
            image_size=arguments.image_size,
            device=arguments.device,
        )
        print(f"  Saved {onnx_path}")

    license_name = arguments.license or "AGPL-3.0-only"
    origin = arguments.origin or model_info["default_origin"]
    if manifest_path.exists() and not arguments.overwrite:
        print(f"  {manifest_path.name} already exists (use --overwrite to replace)")
    else:
        print(f"  Creating manifest {manifest_path.name} ...")
        _create_manifest(
            onnx_path,
            manifest_path,
            model_name=model_name,
            model_info=model_info,
            image_size=arguments.image_size,
            origin=origin,
            license_name=license_name,
            classes=arguments.classes,
        )
        print(f"  Saved {manifest_path}")

    return _print_done(output_dir, [pt_path, onnx_path, manifest_path], onnx_path, manifest_path)


def _handle_registry_pt(
    model_name: str,
    registry_info: dict[str, Any],
    arguments: argparse.Namespace,
) -> int:
    """Registry entry that still needs Ultralytics .pt → ONNX export."""
    pt_url = str(registry_info.get("pt_url") or registry_info.get("url") or "")
    expected_sha = _required_registry_sha256(model_name, registry_info)
    model_info = {
        "description": str(registry_info.get("description", model_name)),
        "pt_url": pt_url,
        "default_origin": pt_url,
        "task": str(registry_info.get("task", "segment")),
    }
    # Temporarily inject into the downloadable path shape.
    output_dir = Path(arguments.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    pt_path = output_dir / f"{model_name}.pt"
    onnx_path = output_dir / f"{model_name}.onnx"
    manifest_path = output_dir / f"{model_name}.model.json"
    image_size = int(registry_info.get("imgsz") or arguments.image_size)

    if pt_path.exists() and not arguments.overwrite:
        _verify_or_remove(pt_path, expected_sha256=expected_sha)
        print(f"  {pt_path.name} already exists (use --overwrite to replace)")
    else:
        print(f"  Downloading {pt_url} ...")
        try:
            _download_https(pt_url, pt_path, expected_sha256=expected_sha)
        except Exception as error:
            raise DownloadModelError(f"download failed for {model_name}: {error}") from error
        print(f"  Saved {pt_path}")

    if onnx_path.exists() and not arguments.overwrite:
        print(f"  {onnx_path.name} already exists (use --overwrite to replace)")
    else:
        print(f"  Exporting {pt_path.name} to ONNX (imgsz={image_size}) ...")
        _export_onnx(pt_path, onnx_path, image_size=image_size, device=arguments.device)
        print(f"  Saved {onnx_path}")

    classes = arguments.classes
    if classes is None:
        classes = _registry_classes_as_list(registry_info)

    # When neither --classes nor registry classes are available, create_manifest
    # falls back to COCO 80 defaults (classes=None) — intended for .pt teachers.
    license_name = arguments.license or str(registry_info.get("license") or "AGPL-3.0-only")
    origin = arguments.origin or pt_url
    if manifest_path.exists() and not arguments.overwrite:
        print(f"  {manifest_path.name} already exists (use --overwrite to replace)")
    else:
        print(f"  Creating manifest {manifest_path.name} ...")
        _create_manifest(
            onnx_path,
            manifest_path,
            model_name=model_name,
            model_info=model_info,
            image_size=image_size,
            origin=origin,
            license_name=license_name,
            classes=classes,
        )
        print(f"  Saved {manifest_path}")

    return _print_done(output_dir, [pt_path, onnx_path, manifest_path], onnx_path, manifest_path)


def _handle_onnx_direct(
    model_name: str,
    registry_info: dict[str, Any],
    arguments: argparse.Namespace,
) -> int:
    """Download a ready-made ONNX file; write manifest from registry classes."""
    url = str(registry_info.get("url") or "")
    if not url:
        print(f"error: registry model {model_name!r} has no url")
        return 2
    expected_sha = _required_registry_sha256(model_name, registry_info)

    output_dir = Path(arguments.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    # Prefer URL basename for compatibility with prepare/cs2_sam defaults;
    # also write a stable name from the registry key when different.
    url_name = _url_basename(url)
    if not url_name.lower().endswith(".onnx"):
        url_name = f"{model_name}.onnx"
    onnx_path = output_dir / url_name
    alias_path = output_dir / f"{model_name}.onnx"
    manifest_path = output_dir / f"{model_name}.model.json"

    if onnx_path.exists() and not arguments.overwrite:
        _verify_or_remove(onnx_path, expected_sha256=expected_sha)
        print(f"  {onnx_path.name} already exists (use --overwrite to replace)")
    else:
        print(f"  Downloading {url} ...")
        try:
            _download_https(url, onnx_path, expected_sha256=expected_sha)
        except Exception as error:
            raise DownloadModelError(f"download failed for {model_name}: {error}") from error
        print(f"  Saved {onnx_path}")

    actual = _verify_or_remove(onnx_path, expected_sha256=expected_sha)
    print(f"  SHA-256 verified: {actual[:16]}…")

    # Convenience alias when URL basename differs from the registry key.
    if alias_path.resolve() != onnx_path.resolve():
        if alias_path.exists() and not arguments.overwrite:
            _verify_or_remove(alias_path, expected_sha256=expected_sha)
        else:
            try:
                if alias_path.exists():
                    alias_path.unlink()
                # Hard link when possible; fall back to copy.
                try:
                    alias_path.hardlink_to(onnx_path)
                except OSError:
                    import shutil

                    shutil.copy2(onnx_path, alias_path)
                print(f"  Alias: {alias_path.name} -> {onnx_path.name}")
            except OSError as error:
                print(f"  (could not create alias {alias_path.name}: {error})")

    classes = arguments.classes
    if classes is None:
        classes = _registry_classes_as_list(registry_info)

    license_name = arguments.license or str(registry_info.get("license") or "AGPL-3.0-only")
    origin = arguments.origin or url
    image_size = int(registry_info.get("imgsz") or arguments.image_size)
    manifest_to_print: Path | None = manifest_path

    if classes is None:
        # ONNX-direct entries with no class metadata skip the manifest entirely;
        # unlike the .pt paths, create_manifest(classes=None) must NOT apply COCO
        # defaults here — these models (EdgeSAM etc.) have no COCO task mapping.
        print("  Skipping manifest (no classes in registry; pass --classes to create one)")
        manifest_to_print = None
    elif manifest_path.exists() and not arguments.overwrite:
        print(f"  {manifest_path.name} already exists (use --overwrite to replace)")
    else:
        print(f"  Creating manifest {manifest_path.name} ...")
        _create_manifest(
            onnx_path,
            manifest_path,
            model_name=model_name,
            model_info={
                "task": str(registry_info.get("task", "detect")),
                "default_origin": origin,
            },
            image_size=image_size,
            origin=origin,
            license_name=license_name,
            classes=classes,
        )
        print(f"  Saved {manifest_path}")
        manifest_to_print = manifest_path

    files = [onnx_path]
    if alias_path.exists() and alias_path.resolve() != onnx_path.resolve():
        files.append(alias_path)
    if manifest_to_print is not None and manifest_to_print.exists():
        files.append(manifest_to_print)
    return _print_done(output_dir, files, onnx_path, manifest_to_print)


def _print_done(
    output_dir: Path,
    files: list[Path],
    onnx_path: Path,
    manifest_path: Path | None,
) -> int:
    print(f"\nDone. Files in {output_dir}:")
    for f in files:
        if f.exists():
            size_mb = f.stat().st_size / (1024 * 1024)
            print(f"  {f.name}  ({size_mb:.1f} MB)")

    print("\nUse with:")
    if manifest_path is not None and manifest_path.exists():
        print("  cs2-vision outline --input video.mp4 \\")
        print(f"    --model {onnx_path} \\")
        print(f"    --manifest {manifest_path} \\")
        print("    --class-name person \\")
        print("    --outline-preset maximum-visibility \\")
        print("    --output artifacts/outlined.mp4")
    else:
        print(f"  # ONNX ready at {onnx_path} (no manifest; use as encoder/decoder weights)")
    return 0


def _export_onnx(
    pt_path: Path,
    onnx_path: Path,
    *,
    image_size: int,
    device: str,
) -> None:
    """Thin wrapper over ``export_onnx`` that raises ``DownloadModelError``."""
    try:
        export_onnx(pt_path, onnx_path, image_size=image_size, device=device)
    except Exception as error:
        raise DownloadModelError(str(error)) from error
