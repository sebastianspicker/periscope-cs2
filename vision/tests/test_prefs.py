from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.prefs import (
    DEFAULT_PRESET,
    SCHEMA_VERSION,
    OutlinePreferences,
    OutlinePreferencesError,
    default_outline_preferences,
    load_outline_preferences,
    load_outline_style,
    save_outline_preferences,
)
from cs2_vision_access.renderer import OutlineStyle, get_outline_preset


class OutlinePreferencesTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.path = self.root / "outline-prefs.json"

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_defaults_equal_high_visibility(self) -> None:
        prefs = default_outline_preferences()
        style = prefs.resolve()

        self.assertEqual(prefs.preset, DEFAULT_PRESET)
        self.assertEqual(prefs.schema_version, SCHEMA_VERSION)
        self.assertEqual(style, get_outline_preset("high-visibility"))
        self.assertEqual(style, OutlineStyle())

    def test_round_trip_default_preferences(self) -> None:
        original = default_outline_preferences()
        save_outline_preferences(self.path, original)
        loaded = load_outline_preferences(self.path)

        self.assertEqual(loaded, original)
        self.assertEqual(loaded.resolve(), get_outline_preset("high-visibility"))

    def test_round_trip_overrides_and_fixed_widths(self) -> None:
        original = OutlinePreferences(
            preset="cyan-black",
            inner_color="#00FFAA",
            outer_color="#000000",
            inner_width=4,
            outer_width=10,
            fill_opacity=0.12,
            scale_with_frame=False,
            stroke_pattern="dashed",
            dash_period_px=16,
            fill_mode="halo",
            halo_blur=11,
            adapt_width_to_area=True,
            outline_kernel="distance",
        )
        save_outline_preferences(self.path, original)
        loaded = load_outline_preferences(self.path)
        style = loaded.resolve()

        self.assertEqual(loaded, original)
        self.assertEqual(style.inner_color, "#00FFAA")
        self.assertEqual(style.outer_color, "#000000")
        self.assertEqual(style.inner_width, 4)
        self.assertEqual(style.outer_width, 10)
        self.assertEqual(style.fill_opacity, 0.12)
        self.assertFalse(style.scale_with_frame)
        self.assertEqual(style.stroke_pattern, "dashed")
        self.assertEqual(style.dash_period_px, 16)
        self.assertEqual(style.fill_mode, "halo")
        self.assertEqual(style.halo_blur, 11)
        self.assertTrue(style.adapt_width_to_area)
        self.assertEqual(style.outline_kernel, "distance")

        payload = json.loads(self.path.read_text(encoding="utf-8"))
        self.assertEqual(payload["schema_version"], SCHEMA_VERSION)
        self.assertEqual(payload["preset"], "cyan-black")
        self.assertFalse(payload["scale_with_frame"])
        self.assertEqual(payload["stroke_pattern"], "dashed")
        self.assertEqual(payload["dash_period_px"], 16)
        self.assertEqual(payload["fill_mode"], "halo")
        self.assertEqual(payload["outline_kernel"], "distance")

    def test_round_trip_outline_style(self) -> None:
        style = OutlineStyle(
            inner_color="#FFFFFF",
            outer_color="#000000",
            inner_width=5,
            outer_width=13,
            fill_opacity=0.15,
            scale_with_frame=True,
            stroke_pattern="dotted",
            dash_period_px=10,
            fill_mode="halo",
            halo_blur=7,
            adapt_width_to_area=True,
            outline_kernel="distance",
        )
        save_outline_preferences(self.path, style)
        loaded_style = load_outline_style(self.path)

        self.assertEqual(loaded_style, style)

    def test_invalid_stroke_pattern_fails_closed(self) -> None:
        with self.assertRaisesRegex(OutlinePreferencesError, "stroke_pattern"):
            OutlinePreferences(stroke_pattern="pulse")
        with self.assertRaisesRegex(OutlinePreferencesError, "dash_period_px"):
            OutlinePreferences(dash_period_px=0)

    def test_invalid_fill_mode_and_kernel_fail_closed(self) -> None:
        with self.assertRaisesRegex(OutlinePreferencesError, "fill_mode"):
            OutlinePreferences(fill_mode="glow")
        with self.assertRaisesRegex(OutlinePreferencesError, "outline_kernel"):
            OutlinePreferences(outline_kernel="flood")
        with self.assertRaisesRegex(OutlinePreferencesError, "halo_blur"):
            OutlinePreferences(halo_blur=0)
        with self.assertRaisesRegex(OutlinePreferencesError, "adapt_width_to_area"):
            OutlinePreferences(adapt_width_to_area="yes")  # type: ignore[arg-type]

    def test_partial_overrides_fall_back_to_preset(self) -> None:
        base = get_outline_preset("maximum-visibility")
        prefs = OutlinePreferences(preset="maximum-visibility", fill_opacity=0.2)
        style = prefs.resolve()

        self.assertEqual(style.inner_color, base.inner_color)
        self.assertEqual(style.outer_color, base.outer_color)
        self.assertEqual(style.inner_width, base.inner_width)
        self.assertEqual(style.outer_width, base.outer_width)
        self.assertEqual(style.fill_opacity, 0.2)
        self.assertEqual(style.scale_with_frame, base.scale_with_frame)

    def test_invalid_contrast_fails_closed(self) -> None:
        with self.assertRaisesRegex(OutlinePreferencesError, "contrast ratio"):
            OutlinePreferences(
                preset="high-visibility",
                inner_color="#777777",
                outer_color="#777777",
            )

        save_outline_preferences(self.path, default_outline_preferences())
        payload = json.loads(self.path.read_text(encoding="utf-8"))
        payload["inner_color"] = "#888888"
        payload["outer_color"] = "#888888"
        self.path.write_text(json.dumps(payload), encoding="utf-8")

        with self.assertRaisesRegex(OutlinePreferencesError, "contrast ratio"):
            load_outline_preferences(self.path)

    def test_unknown_keys_fail_closed(self) -> None:
        save_outline_preferences(self.path, default_outline_preferences())
        payload = json.loads(self.path.read_text(encoding="utf-8"))
        payload["download_url"] = "https://invalid.example/prefs"
        self.path.write_text(json.dumps(payload), encoding="utf-8")

        with self.assertRaisesRegex(OutlinePreferencesError, "unknown"):
            load_outline_preferences(self.path)

    def test_unknown_preset_fails_closed(self) -> None:
        with self.assertRaisesRegex(OutlinePreferencesError, "unknown outline preset"):
            OutlinePreferences(preset="not-a-real-preset")

    def test_fill_and_width_geometry_fail_closed(self) -> None:
        with self.assertRaisesRegex(OutlinePreferencesError, "fill_opacity"):
            OutlinePreferences(fill_opacity=0.9)
        with self.assertRaisesRegex(OutlinePreferencesError, "outer_width"):
            OutlinePreferences(inner_width=4, outer_width=5)
        with self.assertRaisesRegex(OutlinePreferencesError, "inner_width"):
            OutlinePreferences(inner_width=0)

    def test_missing_required_keys_fail_closed(self) -> None:
        self.path.write_text(json.dumps({"preset": "high-visibility"}), encoding="utf-8")
        with self.assertRaisesRegex(OutlinePreferencesError, "missing"):
            load_outline_preferences(self.path)

    def test_invalid_field_types_fail_closed(self) -> None:
        save_outline_preferences(self.path, default_outline_preferences())
        payload = json.loads(self.path.read_text(encoding="utf-8"))
        payload["inner_width"] = True
        self.path.write_text(json.dumps(payload), encoding="utf-8")
        with self.assertRaisesRegex(OutlinePreferencesError, "inner_width"):
            load_outline_preferences(self.path)

        payload["inner_width"] = None
        payload["scale_with_frame"] = "yes"
        self.path.write_text(json.dumps(payload), encoding="utf-8")
        with self.assertRaisesRegex(OutlinePreferencesError, "scale_with_frame"):
            load_outline_preferences(self.path)

    def test_constructor_rejects_unloadable_override_types(self) -> None:
        with self.assertRaisesRegex(OutlinePreferencesError, "inner_width"):
            OutlinePreferences(inner_width=True)
        with self.assertRaisesRegex(OutlinePreferencesError, "inner_width"):
            OutlinePreferences(inner_width=3.5)
        with self.assertRaisesRegex(OutlinePreferencesError, "inner_width"):
            OutlinePreferences(inner_width="3")
        with self.assertRaisesRegex(OutlinePreferencesError, "scale_with_frame"):
            OutlinePreferences(scale_with_frame="yes")
        with self.assertRaisesRegex(OutlinePreferencesError, "fill_opacity"):
            OutlinePreferences(fill_opacity="0.1")
        with self.assertRaisesRegex(OutlinePreferencesError, "inner_width"):
            OutlinePreferences.from_style(
                OutlineStyle(inner_width=3.5, outer_width=8)  # type: ignore[arg-type]
            )

    def test_wrong_schema_version_fails_closed(self) -> None:
        with self.assertRaisesRegex(OutlinePreferencesError, "schema_version"):
            OutlinePreferences(schema_version=0)
        save_outline_preferences(self.path, default_outline_preferences())
        payload = json.loads(self.path.read_text(encoding="utf-8"))
        payload["schema_version"] = 2
        self.path.write_text(json.dumps(payload), encoding="utf-8")
        with self.assertRaisesRegex(OutlinePreferencesError, "schema_version"):
            load_outline_preferences(self.path)

    def test_non_json_extension_fails_closed(self) -> None:
        text_path = self.root / "prefs.txt"
        with self.assertRaisesRegex(OutlinePreferencesError, r"\.json"):
            save_outline_preferences(text_path, default_outline_preferences())
        text_path.write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(OutlinePreferencesError, r"\.json"):
            load_outline_preferences(text_path)

    def test_directory_destination_fails_closed(self) -> None:
        directory = self.root / "prefs-dir.json"
        directory.mkdir()
        with self.assertRaisesRegex(OutlinePreferencesError, "not a regular file"):
            save_outline_preferences(directory, default_outline_preferences(), overwrite=True)

    def test_parent_symlink_is_rejected(self) -> None:
        real_parent = self.root / "real-parent"
        real_parent.mkdir()
        link_parent = self.root / "link-parent"
        try:
            os.symlink(real_parent, link_parent)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable in this environment")

        linked_path = link_parent / "outline-prefs.json"
        with self.assertRaisesRegex(OutlinePreferencesError, "parent"):
            save_outline_preferences(linked_path, default_outline_preferences())

        real_file = real_parent / "outline-prefs.json"
        save_outline_preferences(real_file, default_outline_preferences())
        with self.assertRaisesRegex(OutlinePreferencesError, "parent"):
            load_outline_preferences(linked_path)

    def test_overwrite_required_when_file_exists(self) -> None:
        save_outline_preferences(self.path, default_outline_preferences())
        with self.assertRaisesRegex(OutlinePreferencesError, "already exist"):
            save_outline_preferences(self.path, default_outline_preferences())
        save_outline_preferences(
            self.path,
            OutlinePreferences(preset="maximum-visibility"),
            overwrite=True,
        )
        self.assertEqual(
            load_outline_preferences(self.path).preset,
            "maximum-visibility",
        )

    def test_symlink_destination_is_rejected(self) -> None:
        target = self.root / "target.json"
        target.write_text("{}", encoding="utf-8")
        link = self.root / "link.json"
        try:
            os.symlink(target, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable in this environment")

        with self.assertRaisesRegex(OutlinePreferencesError, "symlink"):
            save_outline_preferences(link, default_outline_preferences(), overwrite=True)
        with self.assertRaisesRegex(OutlinePreferencesError, "symlink"):
            load_outline_preferences(link)

    def test_missing_file_fails_closed(self) -> None:
        with self.assertRaisesRegex(OutlinePreferencesError, "not a regular file"):
            load_outline_preferences(self.root / "missing.json")


if __name__ == "__main__":
    unittest.main()
