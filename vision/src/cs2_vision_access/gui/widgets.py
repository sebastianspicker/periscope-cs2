"""Reusable tkinter widget helpers for the dashboard (tkinter used only in methods)."""

from __future__ import annotations

from typing import Any


class WidgetHelpersMixin:
    """Grid label+entry pairs and scale rows with live value labels."""

    def _grid_pair(self, parent: Any, row: int, text: str, variable: Any) -> None:
        self._ttk.Label(parent, text=text).grid(row=row, column=0, sticky="w", pady=2)
        self._ttk.Entry(parent, textvariable=variable, width=16).grid(
            row=row, column=1, sticky="w", padx=6
        )

    def _scale_row(
        self,
        parent: Any,
        row: int,
        text: str,
        variable: Any,
        from_: float,
        to: float,
        resolution: float,
        fmt: str,
    ) -> None:
        self._ttk.Label(parent, text=text).grid(row=row, column=0, sticky="w", pady=2)
        value_var = self._tk.StringVar(master=self._root)

        def _sync_value(*_args: object) -> None:
            try:
                value_var.set(fmt.format(float(variable.get())))
            except (ValueError, TypeError):
                value_var.set("")

        scale = self._ttk.Scale(
            parent,
            from_=from_,
            to=to,
            variable=variable,
            resolution=resolution,
            length=220,
        )
        scale.grid(row=row, column=1, sticky="we", padx=6)
        self._ttk.Label(parent, textvariable=value_var, width=6).grid(row=row, column=2, sticky="w")
        variable.trace_add("write", _sync_value)
        _sync_value()
