from __future__ import annotations

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import MagicMock, patch

from cs2_vision_access.inference.optimize import suggest_input_size


class SuggestInputSizeTests(unittest.TestCase):
    def test_suggest_input_size_returns_multiple_of_32(self) -> None:
        size = suggest_input_size(target_fps=30.0, device="cuda")
        self.assertEqual(size % 32, 0)

    def test_suggest_input_size_cpu_returns_smaller(self) -> None:
        cpu_size = suggest_input_size(target_fps=30.0, device="cpu")
        cuda_size = suggest_input_size(target_fps=30.0, device="cuda")
        self.assertLessEqual(cpu_size, cuda_size)

    def test_suggest_input_size_low_fps_target(self) -> None:
        size = suggest_input_size(target_fps=5.0, device="cpu")
        self.assertEqual(size % 32, 0)
        self.assertGreaterEqual(size, 320)

    def test_suggest_input_size_unknown_device_defaults_to_cpu(self) -> None:
        size = suggest_input_size(target_fps=5.0, device="unknown")
        self.assertGreaterEqual(size, 320)

    def test_suggest_input_size_explicit_model_params(self) -> None:
        size = suggest_input_size(target_fps=30.0, device="cuda", model_params_mb=50.0)
        self.assertEqual(size % 32, 0)

    def test_suggest_input_size_does_not_exceed_768(self) -> None:
        size = suggest_input_size(target_fps=1.0, device="tensorrt")
        self.assertLessEqual(size, 768)

    def test_suggest_input_size_slow_measured_fps_shrinks_result(self) -> None:
        default = suggest_input_size(target_fps=30.0, device="cuda")
        measured = suggest_input_size(target_fps=30.0, device="cuda", measured_fps_640=6.0)
        self.assertEqual(measured, 320)
        self.assertLess(measured, default)

    def test_suggest_input_size_fast_measured_fps_returns_max(self) -> None:
        size = suggest_input_size(target_fps=30.0, device="cuda", measured_fps_640=240.0)
        self.assertEqual(size, 768)

    def test_suggest_input_size_model_params_affects_result(self) -> None:
        tiny = suggest_input_size(target_fps=30.0, device="cuda", model_params_mb=2.0)
        huge = suggest_input_size(target_fps=30.0, device="cuda", model_params_mb=200.0)
        self.assertGreater(tiny, huge)

    def test_suggest_input_size_non_positive_target_raises(self) -> None:
        with self.assertRaises(ValueError):
            suggest_input_size(target_fps=0.0, device="cuda")
        with self.assertRaises(ValueError):
            suggest_input_size(target_fps=-5.0, device="cuda")

    def test_suggest_input_size_non_positive_measured_fps_raises(self) -> None:
        with self.assertRaises(ValueError):
            suggest_input_size(target_fps=30.0, device="cuda", measured_fps_640=0.0)

    def test_suggest_input_size_non_positive_params_fall_back_to_default(self) -> None:
        default = suggest_input_size(target_fps=30.0, device="cuda")
        self.assertEqual(
            suggest_input_size(target_fps=30.0, device="cuda", model_params_mb=-1.0),
            default,
        )
        self.assertEqual(
            suggest_input_size(target_fps=30.0, device="cuda", model_params_mb=0.0),
            default,
        )


class ConvertToFp16Tests(unittest.TestCase):
    def test_convert_to_fp16_raises_when_missing_deps(self) -> None:
        with patch.dict("sys.modules", {"onnx": None, "onnxconverter_common": None}):
            from cs2_vision_access.inference.optimize import convert_to_fp16

            with self.assertRaises(ImportError):
                convert_to_fp16("model.onnx")

    def test_convert_to_fp16_accepts_string_existing_output(self) -> None:
        with TemporaryDirectory() as temp_dir:
            existing = Path(temp_dir) / "model-fp16.onnx"
            existing.touch()
            with patch.dict(
                "sys.modules",
                {
                    "onnx": MagicMock(),
                    "onnxconverter_common": MagicMock(),
                },
            ):
                from cs2_vision_access.inference.optimize import convert_to_fp16

                with patch("cs2_vision_access.inference.optimize.logger") as mock_log:
                    result = convert_to_fp16("model.onnx", output_path=str(existing))
                    mock_log.info.assert_called_once()
                    self.assertEqual(result, existing)


class OptimizeOnnxModelTests(unittest.TestCase):
    def test_optimize_accepts_string_output_path(self) -> None:
        with TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "model.ort"
            ort = MagicMock()

            def create_optimized_model(*args, sess_options, **kwargs):
                del args, kwargs
                Path(sess_options.optimized_model_filepath).write_bytes(b"ort")

            ort.InferenceSession.side_effect = create_optimized_model
            with patch.dict("sys.modules", {"onnxruntime": ort}):
                from cs2_vision_access.inference.optimize import optimize_onnx_model

                result = optimize_onnx_model("model.onnx", output_path=str(output_path))
                self.assertEqual(result, output_path)
                self.assertTrue(output_path.is_file())

    def test_optimize_skips_existing_output(self) -> None:
        with TemporaryDirectory() as temp_dir:
            existing = Path(temp_dir) / "model.ort"
            existing.touch()
            with patch.dict("sys.modules", {"onnxruntime": MagicMock()}):
                from cs2_vision_access.inference.optimize import optimize_onnx_model

                with patch("cs2_vision_access.inference.optimize.logger") as mock_log:
                    result = optimize_onnx_model("model.onnx", output_path=str(existing))
                    mock_log.info.assert_called_once()
                    self.assertEqual(result, existing)


class OptimizeForGpuTests(unittest.TestCase):
    def test_optimize_for_gpu_default_output_dir(self) -> None:
        from cs2_vision_access.inference.optimize import optimize_for_gpu

        with (
            patch(
                "cs2_vision_access.inference.optimize.convert_to_fp16",
                return_value=Path("model-fp16.onnx"),
            ),
            patch(
                "cs2_vision_access.inference.optimize.optimize_onnx_model",
                return_value=Path("model.ort"),
            ),
        ):
            results = optimize_for_gpu("model.onnx", convert_fp16=True, build_ort=True)
            self.assertIn("fp16", results)
            self.assertIn("ort", results)
            self.assertIn("fp16_ort", results)
