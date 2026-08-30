"""WCAG relative luminance and contrast helpers for evaluation metrics."""

from __future__ import annotations

from cs2_vision_access.domain.outline import parse_hex_bgr


def _hex_relative_luminance(value: str) -> float:
    blue, green, red = parse_hex_bgr(value)
    return _channel_relative_luminance(red, green, blue)


def _bgr_relative_luminance(blue: float, green: float, red: float) -> float:
    return _channel_relative_luminance(red, green, blue)


def _channel_relative_luminance(red: float, green: float, blue: float) -> float:
    def linear(channel: float) -> float:
        normalised = max(0.0, min(255.0, channel)) / 255.0
        if normalised <= 0.04045:
            return normalised / 12.92
        return float(((normalised + 0.055) / 1.055) ** 2.4)

    return float(0.2126 * linear(red) + 0.7152 * linear(green) + 0.0722 * linear(blue))


def _luminance_contrast_ratio(first: float, second: float) -> float:
    lighter = max(first, second)
    darker = min(first, second)
    return (lighter + 0.05) / (darker + 0.05)
