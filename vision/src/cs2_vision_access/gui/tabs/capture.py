"""Capture tab construction for the desktop GUI dashboard."""

from __future__ import annotations

from typing import Any

from cs2_vision_access.gui.constants import CAPTURE_BACKENDS


class CaptureTabMixin:
    """Build the Capture notebook tab."""

    def _build_capture_tab(self, notebook: Any) -> None:
        ttk = self._ttk
        tab = ttk.Frame(notebook, padding=10)
        notebook.add(tab, text="Capture")

        source_frame = ttk.LabelFrame(tab, text="Source", padding=8)
        source_frame.pack(fill="x")
        for value, label in (
            ("screen", "Screen capture"),
            ("capture-device", "Capture device"),
            ("file", "Video file"),
        ):
            ttk.Radiobutton(source_frame, text=label, value=value, variable=self._source_type).pack(
                side="left", padx=8
            )

        screen_frame = ttk.LabelFrame(tab, text="Screen capture", padding=8)
        screen_frame.pack(fill="x", pady=(8, 0))
        ttk.Label(screen_frame, text="Monitor").pack(side="left")
        self._monitor_combo = ttk.Combobox(
            screen_frame, textvariable=self._monitor_var, state="readonly", width=36
        )
        self._monitor_combo.pack(side="left", padx=4)
        self._monitor_combo.bind("<<ComboboxSelected>>", self._on_monitor_selected)
        ttk.Button(screen_frame, text="Refresh", command=self._refresh_monitors).pack(
            side="left", padx=4
        )
        self._refresh_monitors()

        device_frame = ttk.LabelFrame(tab, text="Capture device", padding=8)
        device_frame.pack(fill="x", pady=(8, 0))
        ttk.Label(device_frame, text="Device").pack(side="left")
        self._device_combo = ttk.Combobox(
            device_frame, textvariable=self._device_var, state="readonly", width=36
        )
        self._device_combo.pack(side="left", padx=4)
        ttk.Button(device_frame, text="Refresh", command=self._refresh_devices).pack(
            side="left", padx=4
        )
        self._refresh_devices()

        file_frame = ttk.LabelFrame(tab, text="Video file", padding=8)
        file_frame.pack(fill="x", pady=(8, 0))
        ttk.Entry(file_frame, textvariable=self._file_path, width=52).pack(side="left")
        ttk.Button(file_frame, text="Browse...", command=self._browse_file).pack(
            side="left", padx=4
        )

        params = ttk.LabelFrame(tab, text="Capture parameters", padding=8)
        params.pack(fill="x", pady=(8, 0))
        self._grid_pair(params, 0, "Width", self._cap_width)
        self._grid_pair(params, 1, "Height", self._cap_height)
        self._grid_pair(params, 2, "FPS", self._cap_fps)
        ttk.Label(params, text="Backend").grid(row=3, column=0, sticky="w", pady=2)
        self._backend_combo = ttk.Combobox(
            params,
            textvariable=self._backend,
            values=CAPTURE_BACKENDS,
            state="readonly",
            width=14,
        )
        self._backend_combo.grid(row=3, column=1, sticky="w", padx=6)
