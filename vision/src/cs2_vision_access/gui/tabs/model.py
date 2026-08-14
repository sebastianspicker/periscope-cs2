"""Model tab construction for the desktop GUI dashboard."""

from __future__ import annotations

from typing import Any

from cs2_vision_access.segmenters import SUPPORTED_SEGMENTER_BACKENDS


class ModelTabMixin:
    """Build the Model notebook tab."""

    def _build_model_tab(self, notebook: Any) -> None:
        ttk = self._ttk
        tab = ttk.Frame(notebook, padding=10)
        notebook.add(tab, text="Model")

        paths = ttk.LabelFrame(tab, text="Model files", padding=8)
        paths.pack(fill="x")
        ttk.Label(paths, text="Model (.onnx)").grid(row=0, column=0, sticky="w")
        ttk.Entry(paths, textvariable=self._model_path, width=48).grid(row=0, column=1, padx=6)
        ttk.Button(paths, text="Browse...", command=self._browse_model).grid(row=0, column=2)
        ttk.Label(paths, text="Manifest (.json)").grid(row=1, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(paths, textvariable=self._manifest_path, width=48).grid(
            row=1, column=1, padx=6, pady=(6, 0)
        )
        ttk.Button(paths, text="Browse...", command=self._browse_manifest).grid(
            row=1, column=2, pady=(6, 0)
        )

        inference = ttk.LabelFrame(tab, text="Inference", padding=8)
        inference.pack(fill="x", pady=(8, 0))
        ttk.Label(inference, text="Backend").grid(row=0, column=0, sticky="w")
        self._seg_backend_combo = ttk.Combobox(
            inference, textvariable=self._seg_backend, state="readonly", width=20
        )
        self._seg_backend_combo["values"] = tuple(sorted(SUPPORTED_SEGMENTER_BACKENDS))
        self._seg_backend_combo.grid(row=0, column=1, sticky="w", padx=6)
        ttk.Label(inference, text="Class names (comma separated)").grid(
            row=1, column=0, sticky="w", pady=(6, 0)
        )
        ttk.Entry(inference, textvariable=self._class_names, width=40).grid(
            row=1, column=1, sticky="w", padx=6, pady=(6, 0)
        )
        ttk.Label(inference, text="Image size").grid(row=2, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(inference, textvariable=self._image_size, width=16).grid(
            row=2, column=1, sticky="w", padx=6, pady=(6, 0)
        )
        ttk.Label(inference, text="Device").grid(row=3, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(inference, textvariable=self._device, width=16).grid(
            row=3, column=1, sticky="w", padx=6, pady=(6, 0)
        )
        self._scale_row(inference, 4, "Confidence", self._confidence, 0.0, 1.0, 0.01, "{:.2f}")
