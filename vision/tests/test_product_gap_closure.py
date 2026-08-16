from __future__ import annotations

import io
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

import numpy as np


class FileOutputSinkTests(unittest.TestCase):
    def test_directory_path_writes_numbered_pngs(self) -> None:
        from cs2_vision_access.capture.outputs import FileOutputSink

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "seq"
            sink = FileOutputSink(root, fps=30.0)
            frame = np.zeros((8, 8, 3), dtype=np.uint8)
            with patch("cv2.imwrite", return_value=True) as imwrite:
                sink.send(frame, 0)
                sink.send(frame, 1)
            written = [Path(c.args[0]) for c in imwrite.call_args_list]
            self.assertEqual(written[0].name, "frame_000000.png")
            self.assertEqual(written[1].name, "frame_000001.png")
            self.assertEqual(written[0].parent, root)
            sink.close()

    def test_png_suffix_writes_numbered_sequence_not_overwrite(self) -> None:
        from cs2_vision_access.capture.outputs import FileOutputSink

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "out.png"
            sink = FileOutputSink(path, fps=30.0)
            frame = np.zeros((8, 8, 3), dtype=np.uint8)
            with patch("cv2.imwrite", return_value=True) as imwrite:
                sink.send(frame, 0)
                sink.send(frame, 1)
            names = [Path(c.args[0]).name for c in imwrite.call_args_list]
            self.assertEqual(names, ["out_000000.png", "out_000001.png"])
            sink.close()


class SetupModelArtifactGuardTests(unittest.TestCase):
    def test_missing_ultralytics_requires_locked_environment_without_pip(self) -> None:
        import argparse

        from cs2_vision_access.cli.handlers.setup import _download_and_export_model

        with tempfile.TemporaryDirectory() as tmp:
            args = argparse.Namespace(model="yolo11n-seg", skip_download=False)
            output = io.StringIO()
            with (
                patch.dict("sys.modules", {"ultralytics": None}),
                patch("subprocess.check_call") as check_call,
                redirect_stdout(output),
                self.assertRaisesRegex(RuntimeError, "uv sync --frozen --extra train"),
            ):
                _download_and_export_model(args, Path(tmp))

        check_call.assert_not_called()
        self.assertIn("Install the locked training environment", output.getvalue())

    def test_skip_download_fails_when_artifacts_missing(self) -> None:
        import argparse

        from cs2_vision_access.cli.handlers.setup import _download_and_export_model

        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            args = argparse.Namespace(model="yolo11n-seg", skip_download=True)
            with self.assertRaises(FileNotFoundError):
                _download_and_export_model(args, out)

    def test_skip_download_ok_when_artifacts_present(self) -> None:
        import argparse

        from cs2_vision_access.cli.handlers.setup import _download_and_export_model

        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            onnx = out / "yolo11n-seg.onnx"
            man = out / "yolo11n-seg.model.json"
            onnx.write_bytes(b"fake")
            man.write_text("{}", encoding="utf-8")
            args = argparse.Namespace(model="yolo11n-seg", skip_download=True)
            got_onnx, got_man = _download_and_export_model(args, out)
            self.assertEqual(got_onnx, onnx)
            self.assertEqual(got_man, man)


if __name__ == "__main__":
    unittest.main()
