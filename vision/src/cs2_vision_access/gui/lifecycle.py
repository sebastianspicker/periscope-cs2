"""Run lifecycle, config I/O, browse dialogs, and preview polling for GuiApp."""

from __future__ import annotations

import queue
import time
from pathlib import Path
from typing import Any

from cs2_vision_access.config import (
    CONFIG_FILENAME,
    ConfigError,
    load_config,
    save_config,
)
from cs2_vision_access.gui.constants import PREVIEW_POLL_MS
from cs2_vision_access.gui.controller import GuiPipelineController
from cs2_vision_access.gui.model import GuiSettings
from cs2_vision_access.gui.preview import to_preview_bytes


class LifecycleMixin:
    """Start/stop live, hotkeys, preview poll, browse, config load/save, log, close."""

    def _browse_file(self) -> None:
        path = self._filedialog.askopenfilename(
            title="Select video file",
            filetypes=[("Video files", "*.mp4 *.avi *.mkv *.mov *.webm"), ("All files", "*.*")],
        )
        if path:
            self._file_path.set(path)

    def _browse_model(self) -> None:
        path = self._filedialog.askopenfilename(
            title="Select model file",
            filetypes=[("ONNX models", "*.onnx"), ("All files", "*.*")],
        )
        if path:
            self._model_path.set(path)

    def _browse_manifest(self) -> None:
        path = self._filedialog.askopenfilename(
            title="Select model manifest",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if path:
            self._manifest_path.set(path)

    def _browse_sink(self) -> None:
        path = self._filedialog.asksaveasfilename(title="Select output file or directory")
        if path:
            self._output_sink.set(path)

    def start_live(self) -> None:
        if self._controller is not None and self._controller.is_running:
            return
        try:
            settings = self._read_settings()
        except (ValueError, TypeError) as error:
            self._messagebox.showerror("Invalid settings", str(error))
            return
        if settings.source_type == "file" and not settings.file_path:
            self._messagebox.showerror("Missing input", "Set a video file path on the Capture tab.")
            return
        if not settings.model_path:
            self._messagebox.showerror("Missing model", "Set a model path on the Model tab.")
            return
        model = Path(settings.model_path)
        manifest = Path(settings.manifest_path)
        if not model.is_file():
            self._log(f"Warning: model file not found: {model}")
        if not manifest.is_file():
            self._log(f"Warning: manifest file not found: {manifest}")

        controller = GuiPipelineController(settings)
        self._controller = controller
        self._started_at = time.monotonic()
        self._clear_preview_queue()
        controller.start(on_frame=self._on_frame, on_log=self._log)
        if controller.is_running:
            self._status_var.set("Running")
            self._start_button.config(state="disabled")
            self._stop_button.config(state="normal")
            self._set_hotkey_buttons("normal")
            self._log(f"Live pipeline started (source: {settings.source_type})")
        else:
            self._controller = None
            self._status_var.set("Start failed - see log")

    def stop_live(self) -> None:
        controller = self._controller
        if controller is None:
            return
        self._log("Stopping live pipeline...")
        controller.stop()
        summary = controller.last_summary
        if summary is not None:
            text = GuiPipelineController.format_summary(summary)
            self._log(text)
            self._show_summary(text)
        else:
            self._log("Pipeline stopped without a summary.")
        self._controller = None
        self._status_var.set("Stopped")
        self._start_button.config(state="normal")
        self._stop_button.config(state="disabled")
        self._set_hotkey_buttons("disabled")

    def apply_hotkey_button(self, action: str) -> None:
        controller = self._controller
        if controller is None:
            return
        quit_requested = controller.apply_action(action)
        if quit_requested:
            self.stop_live()
            return
        state = controller.control_state
        mode = state.output_mode
        fill = " FILL" if state.alpha_fill else ""
        temporal = "ON" if state.temporal_enabled else "OFF"
        paused = "  |  PAUSED" if state.paused else ""
        self._status_var.set(
            f"Running  |  Preset {state.current_preset}  |  Width offset {state.width_offset:+d}"
            f"  |  Mode {mode}{fill}  |  Temporal {temporal}{paused}"
        )
        self._output_mode.set(mode)
        self._alpha_fill.set(state.alpha_fill)
        self._temporal_enabled.set(state.temporal_enabled)

    def _on_frame(self, rendered: Any, frame_index: int, count: int) -> None:
        png = to_preview_bytes(rendered)
        if png is None:
            return
        try:
            self._preview_queue.put_nowait((png, frame_index, count))
        except queue.Full:
            try:
                self._preview_queue.get_nowait()
            except queue.Empty:
                return
            try:
                self._preview_queue.put_nowait((png, frame_index, count))
            except queue.Full:
                return

    def _poll_preview(self) -> None:
        while True:
            try:
                png, frame_index, count = self._preview_queue.get_nowait()
            except queue.Empty:
                break
            try:
                photo = self._tk.PhotoImage(data=png)
            except Exception:
                continue
            self._last_photo = photo
            self._preview_canvas.itemconfigure(self._preview_image_id, image=photo)
            elapsed = time.monotonic() - self._started_at
            fps = frame_index / elapsed if elapsed > 0 else 0.0
            self._diag_var.set(f"Frame {frame_index}  |  Predictions {count}  |  Avg FPS {fps:.1f}")
        self._root.after(PREVIEW_POLL_MS, self._poll_preview)

    def _clear_preview_queue(self) -> None:
        while True:
            try:
                self._preview_queue.get_nowait()
            except queue.Empty:
                return

    def _set_hotkey_buttons(self, state: str) -> None:
        for button in self._hotkey_buttons:
            button.config(state=state)

    def load_config(self) -> None:
        path = self._filedialog.askopenfilename(
            title="Load configuration",
            initialfile=CONFIG_FILENAME,
            filetypes=[("JSON config", "*.json"), ("All files", "*.*")],
        )
        if not path:
            return
        try:
            config = load_config(path)
        except ConfigError as error:
            self._messagebox.showerror("Load failed", str(error))
            return
        self._apply_settings(GuiSettings.from_config(config))
        self._log(f"Loaded configuration from {path}")

    def save_config(self) -> None:
        path = self._filedialog.asksaveasfilename(
            title="Save configuration",
            defaultextension=".json",
            initialfile=CONFIG_FILENAME,
            filetypes=[("JSON config", "*.json"), ("All files", "*.*")],
        )
        if not path:
            return
        try:
            settings = self._read_settings()
            saved = save_config(settings.to_config(), path, overwrite=True)
        except (ConfigError, ValueError, TypeError) as error:
            self._messagebox.showerror("Save failed", str(error))
            return
        self._log(f"Saved configuration to {saved}")

    def reset_settings(self) -> None:
        self._apply_settings(GuiSettings())
        self._log("Reset all settings to defaults")

    def _log(self, message: str) -> None:
        text = getattr(self, "_log_text", None)
        if text is None:
            return
        timestamp = time.strftime("%H:%M:%S")
        text.insert("end", f"[{timestamp}] {message}\n")
        text.see("end")

    def _show_summary(self, text: str) -> None:
        self._summary_text.configure(state="normal")
        self._summary_text.delete("1.0", "end")
        self._summary_text.insert("1.0", text)
        self._summary_text.configure(state="disabled")

    def on_close(self) -> None:
        controller = self._controller
        if controller is not None:
            controller.stop()
        self._root.destroy()
