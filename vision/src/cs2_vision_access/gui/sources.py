"""Monitor and capture-device enumeration for the dashboard."""

from __future__ import annotations

from typing import Any

from cs2_vision_access.capture import list_capture_devices, list_monitors


class SourceEnumerationMixin:
    """Refresh and select monitors / capture devices."""

    def _refresh_monitors(self) -> None:
        try:
            monitors = list_monitors()
        except Exception as error:
            self._log(f"Could not enumerate monitors: {error}")
            self._monitor_combo["values"] = ()
            self._monitor_indices = []
            self._monitor_labels = []
            return
        labels: list[str] = []
        indices: list[int] = []
        for monitor in monitors:
            index = int(monitor.get("index", 0))
            name = str(monitor.get("name", f"Monitor {index}"))
            width = int(monitor.get("width", 0))
            height = int(monitor.get("height", 0))
            labels.append(f"{index}: {name} ({width}x{height})")
            indices.append(index)
        self._monitor_combo["values"] = labels
        self._monitor_indices = indices
        self._monitor_labels = labels

    def _refresh_devices(self) -> None:
        try:
            devices = list_capture_devices()
        except Exception as error:
            self._log(f"Could not enumerate capture devices: {error}")
            self._device_combo["values"] = ()
            self._device_indices = []
            self._device_labels = []
            return
        labels = [f"{device.index}: {device.name}" for device in devices]
        self._device_combo["values"] = labels
        self._device_indices = [device.index for device in devices]
        self._device_labels = labels

    def _monitor_label(self, index: int) -> str:
        if index in self._monitor_indices:
            position = self._monitor_indices.index(index)
            if position < len(self._monitor_labels):
                return self._monitor_labels[position]
        return f"{index}: Monitor {index}"

    def _selected_monitor_index(self) -> int | None:
        current = self._monitor_var.get()
        for position, label in enumerate(self._monitor_labels):
            if label == current:
                return self._monitor_indices[position]
        return None

    def _device_label(self, index: int) -> str:
        if index in self._device_indices:
            position = self._device_indices.index(index)
            if position < len(self._device_labels):
                return self._device_labels[position]
        return f"{index}: Device {index}"

    def _selected_device_index(self) -> int | None:
        current = self._device_var.get()
        for position, label in enumerate(self._device_labels):
            if label == current:
                return self._device_indices[position]
        return None

    def _on_monitor_selected(self, _event: Any) -> None:
        self._region_override = None
