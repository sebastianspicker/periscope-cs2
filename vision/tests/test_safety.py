from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.renderer import OutlineRenderer
from cs2_vision_access.safety import (
    InputSafetyError,
    validate_image_output,
    validate_video_input,
    validate_video_output,
)
from cs2_vision_access.video import VideoProcessingError, process_video


class FileOnlySafetyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_local_supported_video_is_accepted(self) -> None:
        video = self.root / "demo.mp4"
        video.write_bytes(b"fixture")

        self.assertEqual(validate_video_input(video), video.resolve())

    def test_url_and_capture_tokens_are_not_files(self) -> None:
        for value in ("https://example.invalid/demo.mp4", "screen:0", "camera:0"):
            with self.subTest(value=value):
                with self.assertRaisesRegex(InputSafetyError, "local file"):
                    validate_video_input(value)

    def test_input_symlink_is_rejected(self) -> None:
        target = self.root / "demo.mp4"
        target.write_bytes(b"fixture")
        link = self.root / "linked.mp4"
        try:
            os.symlink(target, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable in this environment")

        with self.assertRaisesRegex(InputSafetyError, "symlink"):
            validate_video_input(link)

    def test_output_requires_explicit_overwrite(self) -> None:
        output = self.root / "outlined.mp4"
        output.write_bytes(b"existing")

        with self.assertRaisesRegex(InputSafetyError, "--overwrite"):
            validate_video_output(output, overwrite=False)
        self.assertEqual(
            validate_video_output(output, overwrite=True),
            output.resolve(),
        )

    def test_image_output_requires_png_and_explicit_overwrite(self) -> None:
        png = self.root / "preview.png"
        with self.assertRaisesRegex(InputSafetyError, r"\.png"):
            validate_image_output(self.root / "preview.jpg", overwrite=False)
        self.assertEqual(validate_image_output(png, overwrite=False), png.resolve())
        png.write_bytes(b"existing")
        with self.assertRaisesRegex(InputSafetyError, "--overwrite"):
            validate_image_output(png, overwrite=False)
        self.assertEqual(validate_image_output(png, overwrite=True), png.resolve())

    def test_input_cannot_be_overwritten_as_output(self) -> None:
        video = self.root / "demo.mp4"
        video.write_bytes(b"fixture")

        with self.assertRaisesRegex(VideoProcessingError, "different files"):
            process_video(
                input_path=video,
                segmenter=object(),  # Rejected before any backend method is used.
                renderer=OutlineRenderer(),
                output_path=video,
                display=False,
                realtime_playback=False,
                max_frames=1,
                overwrite=True,
            )


if __name__ == "__main__":
    unittest.main()
