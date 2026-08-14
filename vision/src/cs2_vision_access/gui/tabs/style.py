"""Style tab construction for the desktop GUI dashboard."""

from __future__ import annotations

from typing import Any

from cs2_vision_access.renderer import FILL_MODES, OUTLINE_PRESETS, STROKE_PATTERNS


class StyleTabMixin:
    """Build the Style notebook tab."""

    def _build_style_tab(self, notebook: Any) -> None:
        ttk = self._ttk
        tab = ttk.Frame(notebook, padding=10)
        notebook.add(tab, text="Style")

        preset_frame = ttk.LabelFrame(tab, text="Preset", padding=8)
        preset_frame.pack(fill="x")
        self._preset_combo = ttk.Combobox(
            preset_frame, textvariable=self._preset, state="readonly", width=20
        )
        self._preset_combo["values"] = tuple(sorted(OUTLINE_PRESETS))
        self._preset_combo.pack(side="left", padx=4)
        self._preset_combo.bind("<<ComboboxSelected>>", self._on_preset_selected)
        ttk.Label(
            preset_frame, textvariable=self._preset_desc, wraplength=520, foreground="#555"
        ).pack(side="left", padx=8)

        colors = ttk.LabelFrame(tab, text="Stroke", padding=8)
        colors.pack(fill="x", pady=(8, 0))
        self._grid_pair(colors, 0, "Inner color (#RRGGBB)", self._inner_color)
        self._grid_pair(colors, 1, "Outer color (#RRGGBB)", self._outer_color)
        self._grid_pair(colors, 2, "Inner width (px)", self._inner_width)
        self._grid_pair(colors, 3, "Outer width (px)", self._outer_width)
        self._scale_row(colors, 4, "Fill opacity", self._fill_opacity, 0.0, 0.35, 0.01, "{:.2f}")

        pattern = ttk.LabelFrame(tab, text="Pattern", padding=8)
        pattern.pack(fill="x", pady=(8, 0))
        ttk.Label(pattern, text="Stroke pattern").grid(row=0, column=0, sticky="w")
        self._stroke_pattern_combo = ttk.Combobox(
            pattern, textvariable=self._stroke_pattern, state="readonly", width=14
        )
        self._stroke_pattern_combo["values"] = tuple(sorted(STROKE_PATTERNS))
        self._stroke_pattern_combo.grid(row=0, column=1, sticky="w", padx=6)
        ttk.Label(pattern, text="Dash period (px)").grid(row=0, column=2, padx=(12, 2))
        self._tk.Spinbox(pattern, from_=1, to=64, textvariable=self._dash_period, width=8).grid(
            row=0, column=3
        )
        ttk.Label(pattern, text="Fill mode").grid(row=1, column=0, sticky="w", pady=(6, 0))
        self._fill_mode_combo = ttk.Combobox(
            pattern, textvariable=self._fill_mode, state="readonly", width=14
        )
        self._fill_mode_combo["values"] = tuple(sorted(FILL_MODES))
        self._fill_mode_combo.grid(row=1, column=1, sticky="w", padx=6, pady=(6, 0))
        ttk.Label(pattern, text="Halo blur (px)").grid(row=1, column=2, padx=(12, 2), pady=(6, 0))
        self._tk.Spinbox(pattern, from_=1, to=32, textvariable=self._halo_blur, width=8).grid(
            row=1, column=3, pady=(6, 0)
        )

        flags = ttk.Frame(tab)
        flags.pack(fill="x", pady=(8, 0))
        ttk.Checkbutton(
            flags, text="Adapt width to instance area", variable=self._adapt_width
        ).pack(side="left", padx=8)
        ttk.Checkbutton(
            flags, text="Fixed widths (disable frame scaling)", variable=self._fixed_widths
        ).pack(side="left", padx=8)
