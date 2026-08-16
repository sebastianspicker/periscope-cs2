"""Adversarial tests for the pinned CS2 signature downloader."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.request import Request

_SCRIPTS_DIR = Path(__file__).resolve().parents[1]
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

import cs2_sig_fetch  # noqa: E402


class PinnedSourceFetchTests(unittest.TestCase):
    def test_rejects_non_https_and_untrusted_source_before_opening(self) -> None:
        for url in (
            "http://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json",
            "https://example.invalid/a2x/cs2-dumper/main/output/offsets.json",
            "https://raw.githubusercontent.com/other/project/offsets.json",
        ):
            with self.subTest(url=url), patch.object(
                cs2_sig_fetch.urllib.request, "build_opener"
            ) as opener:
                with self.assertRaises(ValueError):
                    cs2_sig_fetch.fetch(url)
                opener.assert_not_called()

    def test_rejects_unsafe_redirect_before_following_it(self) -> None:
        handler = cs2_sig_fetch._PinnedSourceRedirectHandler()
        request = Request(
            "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json"
        )
        with self.assertRaises(ValueError):
            handler.redirect_request(
                request,
                fp=None,
                code=302,
                msg="Found",
                headers={},
                newurl="http://example.invalid/offsets.json",
            )

    def test_diff_does_not_depend_on_assertions(self) -> None:
        self.assertEqual(
            cs2_sig_fetch.diff_snapshots({"globals": {"x": 1}}, {"globals": {"x": 2}}),
            ["globals.x: 0x1 -> 0x2"],
        )


if __name__ == "__main__":
    unittest.main()
