"""Run tab construction for the desktop GUI dashboard."""

from __future__ import annotations

from typing import Any


class RunTabMixin:
    """Build the Run notebook tab (controls, preview, log, config buttons)."""

    def _build_run_tab(self, notebook: Any) -> None:
        ttk = self._ttk
        tk = self._tk
        tab = ttk.Frame(notebook, padding=10)
        notebook.add(tab, text="Run")

        controls = ttk.Frame(tab)
        controls.pack(fill="x")
        self._start_button = ttk.Button(controls, text="Start Live", command=self.start_live)
        self._start_button.pack(side="left")
        self._stop_button = ttk.Button(
            controls, text="Stop", command=self.stop_live, state="disabled"
        )
        self._stop_button.pack(side="left", padx=8)
        ttk.Label(controls, textvariable=self._status_var).pack(side="left", padx=12)

        hotkeys = ttk.Frame(tab)
        hotkeys.pack(fill="x", pady=(6, 0))
        for label, action in (
            ("Pause", "pause"),
            ("Preset 1", "preset-1"),
            ("Preset 2", "preset-2"),
            ("Preset 3", "preset-3"),
            ("Width +", "width-up"),
            ("Width -", "width-down"),
            ("Temporal", "temporal"),
            ("Mode", "mode-cycle"),
            ("Fill", "fill"),
            ("Info", "diagnostics"),
        ):
            button = ttk.Button(
                hotkeys,
                text=label,
                command=lambda name=action: self.apply_hotkey_button(name),
            )
            button.pack(side="left", padx=2)
            self._hotkey_buttons.append(button)
        self._set_hotkey_buttons("disabled")

        preview_frame = ttk.LabelFrame(tab, text="Live preview", padding=6)
        preview_frame.pack(fill="both", expand=True, pady=(8, 0))
        self._preview_canvas = tk.Canvas(preview_frame, width=640, height=360, background="#000")
        self._preview_canvas.pack(fill="both", expand=True)
        self._preview_image_id = self._preview_canvas.create_image(0, 0, anchor="nw", image=None)

        ttk.Label(tab, textvariable=self._diag_var).pack(fill="x", pady=(6, 0))

        log_frame = ttk.LabelFrame(tab, text="Log", padding=6)
        log_frame.pack(fill="x", pady=(8, 0))
        self._log_text = tk.Text(log_frame, height=7, width=100, wrap="word")
        scroll = ttk.Scrollbar(log_frame, command=self._log_text.yview)
        self._log_text.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        self._log_text.pack(side="left", fill="both", expand=True)

        summary_frame = ttk.LabelFrame(tab, text="Run summary", padding=6)
        summary_frame.pack(fill="x", pady=(8, 0))
        self._summary_text = tk.Text(summary_frame, height=6, width=100, state="disabled")
        self._summary_text.pack(fill="x")

        config_buttons = ttk.Frame(tab)
        config_buttons.pack(fill="x", pady=(8, 0))
        ttk.Button(config_buttons, text="Save Config", command=self.save_config).pack(side="left")
        ttk.Button(config_buttons, text="Load Config", command=self.load_config).pack(
            side="left", padx=8
        )
        ttk.Button(config_buttons, text="Reset", command=self.reset_settings).pack(side="left")
