"""Overlay / Output tab construction for the desktop GUI dashboard."""

from __future__ import annotations

from typing import Any

from cs2_vision_access.interfaces.gui.app_context import GuiAppContext
from cs2_vision_access.interfaces.gui.constants import OUTPUT_MODES


class OverlayTabMixin(GuiAppContext):
    """Build the Overlay / Output notebook tab."""

    def _build_overlay_tab(self, notebook: Any) -> None:
        ttk = self._ttk
        tab = ttk.Frame(notebook, padding=10)
        notebook.add(tab, text="Overlay / Output")

        overlay_frame = ttk.LabelFrame(tab, text="Overlay window", padding=8)
        overlay_frame.pack(fill="x")
        ttk.Checkbutton(
            overlay_frame,
            text="Enable transparent always-on-top overlay",
            variable=self._overlay_enabled,
        ).grid(row=0, column=0, columnspan=4, sticky="w")
        self._grid_pair(overlay_frame, 1, "X position", self._overlay_x)
        self._grid_pair(overlay_frame, 2, "Y position", self._overlay_y)
        self._grid_pair(overlay_frame, 3, "Monitor (0 = auto)", self._overlay_monitor)
        ttk.Label(overlay_frame, text="Window title").grid(row=4, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(overlay_frame, textvariable=self._window_title, width=46).grid(
            row=4, column=1, columnspan=3, sticky="w", padx=6, pady=(6, 0)
        )

        output = ttk.LabelFrame(tab, text="Output", padding=8)
        output.pack(fill="x", pady=(8, 0))
        ttk.Label(output, text="Output mode").grid(row=0, column=0, sticky="w")
        self._output_mode_combo = ttk.Combobox(
            output, textvariable=self._output_mode, state="readonly", width=12
        )
        self._output_mode_combo["values"] = OUTPUT_MODES
        self._output_mode_combo.grid(row=0, column=1, sticky="w", padx=6)
        ttk.Checkbutton(output, text="Alpha fill", variable=self._alpha_fill).grid(
            row=0, column=2, padx=(12, 0), sticky="w"
        )
        self._scale_row(output, 1, "Display scale", self._display_scale, 0.1, 1.0, 0.05, "{:.2f}")
        ttk.Checkbutton(output, text="Headless (no preview window)", variable=self._headless).grid(
            row=2, column=0, columnspan=2, sticky="w", pady=(6, 0)
        )

        temporal = ttk.LabelFrame(tab, text="Temporal stability", padding=8)
        temporal.pack(fill="x", pady=(8, 0))
        ttk.Checkbutton(
            temporal, text="Suppress unstable detections", variable=self._temporal_enabled
        ).grid(row=0, column=0, sticky="w")
        ttk.Label(temporal, text="Min consecutive frames").grid(row=0, column=1, padx=(12, 2))
        self._tk.Spinbox(
            temporal, from_=1, to=30, textvariable=self._temporal_min_frames, width=6
        ).grid(row=0, column=2)
        ttk.Checkbutton(
            temporal,
            text="Hold last mask across dropouts",
            variable=self._temporal_hold,
        ).grid(row=1, column=0, sticky="w", pady=(6, 0))
        ttk.Label(temporal, text="Max dropout frames").grid(
            row=1, column=1, padx=(12, 2), pady=(6, 0)
        )
        self._tk.Spinbox(
            temporal, from_=0, to=60, textvariable=self._temporal_max_dropout, width=6
        ).grid(row=1, column=2, pady=(6, 0))

        sink = ttk.LabelFrame(tab, text="Output sink", padding=8)
        sink.pack(fill="x", pady=(8, 0))
        ttk.Label(sink, text="File or directory (video / PNG sequence)").pack(side="left")
        ttk.Entry(sink, textvariable=self._output_sink, width=40).pack(side="left", padx=6)
        ttk.Button(sink, text="Browse...", command=self._browse_sink).pack(side="left")
