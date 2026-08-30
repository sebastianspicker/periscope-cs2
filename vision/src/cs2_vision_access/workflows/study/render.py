"""Plan and render local study stimulus packs."""

from __future__ import annotations

import json
import os
import shutil
import tempfile
from collections.abc import Callable
from pathlib import Path

from cs2_vision_access.application.configuration.prefs import load_outline_preferences
from cs2_vision_access.application.ports.rendering import OutlineRenderer
from cs2_vision_access.application.ports.segmentation import Segmenter, create_segmenter
from cs2_vision_access.domain.outline import OutlineStyle
from cs2_vision_access.domain.safety import validate_video_input, validate_video_output
from cs2_vision_access.domain.video import VideoRunSummary
from cs2_vision_access.workflows.study._util import (
    _resolve_existing_path,
    _resolve_path,
    _write_json_atomic,
)
from cs2_vision_access.workflows.study.errors import StudyError
from cs2_vision_access.workflows.study.models import (
    DEFAULT_MAX_FRAMES,
    LIKERT_MAX,
    LIKERT_MIN,
    RATING_BOOL_FIELDS,
    RATING_LIKERT_FIELDS,
    RATING_SCHEMA_VERSION,
    SCHEMA_VERSION,
    StudyInference,
    StudyPackage,
    StudyRenderJob,
)
from cs2_vision_access.workflows.study.parse import resolve_condition_style


def stimulus_id_for(clip_id: str, condition_id: str) -> str:
    """Deterministic stimulus identifier used as the pack filename stem."""
    return f"{clip_id}__{condition_id}"


def plan_study_render(
    package: StudyPackage,
    output_directory: str | Path,
    *,
    check_inputs: bool = True,
) -> tuple[StudyRenderJob, ...]:
    """Expand clip × condition jobs and validate local paths without rendering."""
    if not package.clips or not package.conditions:
        raise StudyError("study package must include clips and conditions")

    out_root = Path(output_directory)
    if out_root.is_symlink():
        raise StudyError("output directory must not be a symlink")
    stimuli_dir = out_root / "stimuli"
    base = package.base_directory
    jobs: list[StudyRenderJob] = []

    inference = package.inference
    for clip in package.clips:
        input_path = _resolve_path(clip.input, base_directory=base)
        if check_inputs:
            try:
                input_path = validate_video_input(input_path)
            except ValueError as error:
                raise StudyError(f"clip {clip.clip_id!r} input invalid: {error}") from error
        for condition in package.conditions:
            style: OutlineStyle | None = None
            max_frames = DEFAULT_MAX_FRAMES
            max_seconds: float | None = None
            if condition.kind == "outline":
                if inference is None:
                    raise StudyError("inference is required for outline conditions")
                style = resolve_condition_style(
                    condition,
                    base_directory=base,
                    prefs_loader=load_outline_preferences,
                )
                max_frames = inference.max_frames
                max_seconds = inference.max_seconds
            stim_id = stimulus_id_for(clip.clip_id, condition.condition_id)
            output_path = stimuli_dir / f"{stim_id}.mp4"
            jobs.append(
                StudyRenderJob(
                    study_id=package.study_id,
                    clip_id=clip.clip_id,
                    condition_id=condition.condition_id,
                    kind=condition.kind,
                    input_path=input_path,
                    output_path=output_path,
                    stimulus_id=stim_id,
                    style=style,
                    max_frames=max_frames,
                    max_seconds=max_seconds,
                )
            )
    return tuple(jobs)


def render_study_pack(
    package: StudyPackage,
    output_directory: str | Path,
    *,
    overwrite: bool = False,
    validate_only: bool = False,
    segmenter_factory: Callable[[StudyInference, Path], Segmenter] | None = None,
    process_video_fn: Callable[..., VideoRunSummary] | None = None,
    renderer_factory: Callable[[OutlineStyle], OutlineRenderer] | None = None,
) -> dict[str, object]:
    """Render or plan a local stimulus pack from a study package.

    Layout::

        {output_directory}/
          pack-manifest.v1.json
          ratings.template.jsonl
          stimuli/{clip_id}__{condition_id}.mp4

    File-only: no network. ``validate_only`` expands jobs and writes the pack
    manifest without decoding video or loading models.
    """
    out_root = Path(output_directory)
    if out_root.is_symlink():
        raise StudyError("output directory must not be a symlink")
    jobs = plan_study_render(
        package,
        out_root,
        check_inputs=not validate_only,
    )

    stimuli_dir = out_root / "stimuli"
    if not validate_only:
        out_root.mkdir(parents=True, exist_ok=True)
        if out_root.is_symlink():
            raise StudyError("output directory must not be a symlink")
        stimuli_dir.mkdir(parents=True, exist_ok=True)
        if stimuli_dir.is_symlink():
            raise StudyError("stimuli directory must not be a symlink")

    inference = package.inference
    segmenter: Segmenter | None = None
    base = package.base_directory
    rendered: list[dict[str, object]] = []
    if process_video_fn is None and not validate_only:
        raise StudyError("a media processor must be supplied at the composition boundary")
    process = process_video_fn

    for job in jobs:
        entry = job.as_json()
        if validate_only:
            entry["status"] = "planned"
            rendered.append(entry)
            continue

        try:
            validate_video_output(job.output_path, overwrite=overwrite)
        except ValueError as error:
            raise StudyError(f"stimulus output invalid for {job.stimulus_id!r}: {error}") from error

        if job.kind == "baseline":
            _copy_video_stimulus(job.input_path, job.output_path, overwrite=overwrite)
            entry["status"] = "copied"
            entry["frames_processed"] = None
            rendered.append(entry)
            continue

        if inference is None or job.style is None:
            raise StudyError(f"outline job {job.stimulus_id!r} missing inference or style")

        if segmenter is None:
            factory = segmenter_factory or _default_segmenter_factory
            segmenter = factory(inference, base)

        if renderer_factory is None:
            raise StudyError("a rendering adapter must be supplied at the composition boundary")
        assert process is not None
        summary = process(
            input_path=job.input_path,
            segmenter=segmenter,
            renderer=renderer_factory(job.style),
            output_path=job.output_path,
            display=False,
            realtime_playback=False,
            max_frames=job.max_frames,
            overwrite=overwrite,
            max_seconds=job.max_seconds,
            cue_log_path=None,
        )
        entry["status"] = "rendered"
        entry["frames_processed"] = summary.frames_processed
        entry["termination_reason"] = summary.termination_reason
        entry["instances_outlined"] = summary.instances_outlined
        rendered.append(entry)

    manifest: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "study_id": package.study_id,
        "title": package.title,
        "description": package.description,
        "validate_only": validate_only,
        "job_count": len(jobs),
        "jobs": rendered,
        "clips": [clip.as_json() for clip in package.clips],
        "conditions": [condition.as_json() for condition in package.conditions],
        "rating_fields": {
            "likert": list(RATING_LIKERT_FIELDS),
            "likert_range": [LIKERT_MIN, LIKERT_MAX],
            "boolean": list(RATING_BOOL_FIELDS),
        },
    }
    if inference is not None:
        manifest["inference"] = inference.as_json()

    if not validate_only:
        _write_json_atomic(out_root / "pack-manifest.v1.json", manifest)
        _write_ratings_template(out_root / "ratings.template.jsonl", package)
        manifest["pack_directory"] = str(out_root.resolve())
        manifest["manifest_path"] = str((out_root / "pack-manifest.v1.json").resolve())
    else:
        # Still useful for operators / tests: emit plan without writing media.
        out_root.mkdir(parents=True, exist_ok=True)
        if not out_root.is_symlink():
            _write_json_atomic(out_root / "pack-manifest.v1.json", manifest)
            manifest["manifest_path"] = str((out_root / "pack-manifest.v1.json").resolve())

    return manifest


def _default_segmenter_factory(inference: StudyInference, base_directory: Path) -> Segmenter:
    model_path = _resolve_existing_path(
        inference.model, base_directory=base_directory, label="model"
    )
    manifest_path = _resolve_existing_path(
        inference.manifest, base_directory=base_directory, label="manifest"
    )
    class_names = None if inference.class_names is None else tuple(inference.class_names)
    return create_segmenter(
        model_path,
        manifest_path,
        backend=inference.backend,
        class_names=class_names,
        confidence=inference.confidence,
        image_size=inference.image_size,
        device=inference.device,
    )


def _copy_video_stimulus(source: Path, destination: Path, *, overwrite: bool) -> None:
    """Copy a baseline clip into the pack without re-encoding."""
    validate_video_input(source)
    validate_video_output(destination, overwrite=overwrite)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "wb",
            dir=destination.parent,
            prefix=f".{destination.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_name = handle.name
        shutil.copyfile(source, temporary_name)
        os.replace(temporary_name, destination)
        temporary_name = None
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)


def _write_ratings_template(path: Path, package: StudyPackage) -> None:
    """Write one example JSONL line documenting the rating schema."""
    example = {
        "schema_version": RATING_SCHEMA_VERSION,
        "participant_id": "P001",
        "session_id": "S001",
        "clip_id": package.clips[0].clip_id,
        "condition_id": package.conditions[0].condition_id,
        "stimulus_id": stimulus_id_for(
            package.clips[0].clip_id, package.conditions[0].condition_id
        ),
        "presented_order": 0,
        "ratings": {
            "usefulness": 3,
            "clutter": 3,
            "comfort": 3,
            "small_player_visibility": 3,
            "error_confusion": 3,
            "would_enable": False,
        },
        "notes": "replace with participant response; delete unused example lines",
    }
    text = json.dumps(example, sort_keys=True) + "\n"
    path.write_text(text, encoding="utf-8")
