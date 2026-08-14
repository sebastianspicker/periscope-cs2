"""Two-stage hybrid segmenter: Vombit CS2 detection → EdgeSAM mask refinement.

Chains two ONNX models into a single pipeline:

  1. **Vombit yolov10n-cs2** — YOLOv10n fine-tuned on 120+ CS2 matches,
     detecting ``ct / ct_head / t / t_head`` bounding boxes.
  2. **EdgeSAM** — lightweight SAM variant from Microsoft. The encoder runs
     once per frame, the decoder runs once per detected box.

Pre-trained weights
-------------------
**Vombit detector (public):**
  https://huggingface.co/Vombit/yolov10n_cs2

**EdgeSAM ONNX (public):**
  https://huggingface.co/chongzhou/EdgeSAM
"""

from __future__ import annotations

from pathlib import Path

import numpy as np

from cs2_vision_access.model_manifest import verify_model
from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.segmenters._ort_utils import (
    detect_io_names,
    find_input_name,
    resolve_model_path,
)
from cs2_vision_access.segmenters._preprocessing import (
    DETECTION_SIZE,
    SAM_IMAGE_SIZE,
    box_to_sam_prompt,
    mask_to_polygon,
    preprocess_sam,
    preprocess_vombit,
)
from cs2_vision_access.segmenters.protocol import (
    CS2_SAM_BACKEND,
    SegmenterError,
    resolve_class_ids,
)

# Default ONNX filenames.
_DEFAULT_ENCODER = "edge_sam_3x_encoder.onnx"
_DEFAULT_DECODER = "edge_sam_3x_decoder.onnx"


class Cs2SamSegmenter:
    """Two-stage CS2 player segmentation: Vombit detection → EdgeSAM masks.

    The detector is a checksum-verified ONNX model passed as ``model_path``
    (with its ``manifest_path``). The EdgeSAM encoder/decoder models are
    discovered by convention or via environment variables.
    """

    def __init__(
        self,
        model_path: str | Path,
        manifest_path: str | Path,
        *,
        class_names: tuple[str, ...] | None,
        confidence: float,
        image_size: int,
        device: str,
        sam_encoder_path: str | Path | None = None,
        sam_decoder_path: str | Path | None = None,
    ) -> None:
        if not 0.0 < confidence <= 1.0:
            raise ValueError("confidence must be in (0, 1]")

        model_path = Path(model_path)
        model_dir = model_path.parent

        # 1. Verify detector model + manifest.
        model, manifest = verify_model(model_path, manifest_path)
        self.manifest = manifest
        self.allowed_ids = resolve_class_ids(manifest, class_names)
        self.confidence = confidence
        self.image_size = image_size
        self.device = device
        self.backend = CS2_SAM_BACKEND

        # 2. Load Vombit detector.
        from cs2_vision_access.inference.providers import create_ort_session

        self._detector = create_ort_session(model_path, device=device)

        # Detect I/O names and class count from the detector model.
        det_io = detect_io_names(
            self._detector,
            expected_input="images",
            expected_output="output0",
            num_classes=len(self.manifest.classes),
        )
        self._det_input_name = det_io["input_name"]
        self._det_output_name = det_io["output_name"]
        self._det_num_classes = det_io["num_classes"]

        # 3. Load EdgeSAM encoder + decoder.
        encoder_path = resolve_model_path(
            sam_encoder_path,
            model_dir / _DEFAULT_ENCODER,
            "CS2_SAM_ENCODER",
            "https://huggingface.co/chongzhou/EdgeSAM",
        )
        decoder_path = resolve_model_path(
            sam_decoder_path,
            model_dir / _DEFAULT_DECODER,
            "CS2_SAM_DECODER",
            "https://huggingface.co/chongzhou/EdgeSAM",
        )

        self._sam_encoder = create_ort_session(encoder_path, device=device)
        self._sam_decoder = create_ort_session(decoder_path, device=device)

        # Discover encoder I/O names.
        enc_inputs = {inp.name for inp in self._sam_encoder.get_inputs()}
        enc_outputs = {out.name for out in self._sam_encoder.get_outputs()}
        self._enc_input_name = find_input_name(enc_inputs, "input")
        self._enc_output_name = find_input_name(enc_outputs, "output")

        # Discover decoder I/O names.
        dec_inputs = {inp.name for inp in self._sam_decoder.get_inputs()}
        dec_outputs = {out.name for out in self._sam_decoder.get_outputs()}
        self._dec_input_names = {
            "image_embeddings": find_input_name(dec_inputs, "image_embeddings", "embed"),
            "point_coords": find_input_name(dec_inputs, "point_coords", "coords"),
            "point_labels": find_input_name(dec_inputs, "point_labels", "labels"),
            "mask_input": find_input_name(dec_inputs, "mask_input", "mask"),
            "has_mask_input": find_input_name(dec_inputs, "has_mask_input", "has_mask"),
            "orig_im_size": find_input_name(dec_inputs, "orig_im_size", "orig_size"),
        }
        self._dec_output_name = find_input_name(dec_outputs, "masks")

        # Persistent decoder buffers.
        self._mask_input = np.zeros((1, 1, 256, 256), dtype=np.float32)
        self._has_mask_input = np.zeros(1, dtype=np.float32)

        # 4. Optional GPU-accelerated detection pipeline.
        self._use_gpu_pipeline = False
        self._gpu_pipeline = None
        self._try_enable_gpu_pipeline()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def predict(self, frame_bgr: np.ndarray, *, frame_index: int) -> tuple[InstanceMask, ...]:
        if frame_index < 0:
            raise ValueError("frame_index must be non-negative")
        if frame_bgr.ndim != 3 or frame_bgr.shape[2] != 3:
            raise ValueError("frame_bgr must have shape (height, width, 3)")

        orig_h, orig_w = frame_bgr.shape[:2]

        # Step 1: Detection.
        boxes = self._detect(frame_bgr, orig_w, orig_h)
        if not boxes:
            return ()

        # Step 2: EdgeSAM encoder (once per frame).
        embeddings = self._encode_frame(frame_bgr)

        # Step 3: EdgeSAM decoder (once per box).
        predictions: list[InstanceMask] = []
        for class_id, class_name, score, box in boxes:
            polygon = self._decode_mask(embeddings, box, (orig_h, orig_w))
            if not polygon:
                continue
            predictions.append(
                InstanceMask(
                    frame_index=frame_index,
                    polygon=polygon,
                    confidence=score,
                    class_id=class_id,
                    class_name=class_name,
                )
            )

        return tuple(predictions)

    # ------------------------------------------------------------------
    # Detection
    # ------------------------------------------------------------------

    def _detect(
        self,
        frame_bgr: np.ndarray,
        orig_w: int,
        orig_h: int,
    ) -> list[tuple[int, str, float, tuple[float, float, float, float]]]:
        # GPU path (pipeline applies confidence; re-check so GPU/CPU stay aligned).
        if self._use_gpu_pipeline and self._gpu_pipeline is not None:
            try:
                result = self._gpu_pipeline.run(frame_bgr)
                results: list = []
                for i in range(result.num_detections):
                    score = float(result.scores[i])
                    if score < self.confidence:
                        continue
                    cid = int(result.class_ids[i])
                    if cid not in self.allowed_ids or cid not in self.manifest.classes:
                        continue
                    x1, y1, x2, y2 = result.boxes[i]
                    results.append(
                        (
                            cid,
                            self.manifest.classes[cid],
                            score,
                            (float(x1), float(y1), float(x2), float(y2)),
                        )
                    )
                return results
            except Exception:
                pass

        # CPU path.
        input_tensor = preprocess_vombit(frame_bgr)

        try:
            outputs = self._detector.run(
                [self._det_output_name],
                {self._det_input_name: input_tensor},
            )
        except Exception as error:
            raise SegmenterError(f"detector inference failed: {error}") from error

        if not outputs or outputs[0] is None:
            return []

        raw: np.ndarray = outputs[0]
        if raw.ndim == 3:
            raw = raw[0]

        nc = self._det_num_classes
        if raw.shape[0] < 4 + nc:
            nc = len(self.manifest.classes)

        box_data = raw[:4, :]
        cls_data = raw[4 : 4 + nc, :]
        scale = float(DETECTION_SIZE)

        cx = box_data[0] / scale * orig_w
        cy = box_data[1] / scale * orig_h
        w = box_data[2] / scale * orig_w
        h = box_data[3] / scale * orig_h

        x1 = (cx - w / 2).clip(0, orig_w)
        y1 = (cy - h / 2).clip(0, orig_h)
        x2 = (cx + w / 2).clip(0, orig_w)
        y2 = (cy + h / 2).clip(0, orig_h)

        class_ids = cls_data.argmax(axis=0)
        scores = cls_data[class_ids, np.arange(cls_data.shape[1])]

        keep = scores >= self.confidence
        if not keep.any():
            return []

        x1_f, y1_f, x2_f, y2_f = x1[keep], y1[keep], x2[keep], y2[keep]
        cls_f = class_ids[keep]
        sc_f = scores[keep]

        box_stack = np.stack([x1_f, y1_f, x2_f, y2_f], axis=1).astype(np.float32)
        from cs2_vision_access.inference.cuda import nms as cuda_nms

        kept_idx = cuda_nms(box_stack, sc_f, 0.5)

        results = []
        for idx in kept_idx:
            cid = int(cls_f[idx])
            if cid not in self.allowed_ids or cid not in self.manifest.classes:
                continue
            results.append(
                (
                    cid,
                    self.manifest.classes[cid],
                    float(sc_f[idx]),
                    (float(x1_f[idx]), float(y1_f[idx]), float(x2_f[idx]), float(y2_f[idx])),
                )
            )

        return results

    # ------------------------------------------------------------------
    # EdgeSAM
    # ------------------------------------------------------------------

    def _encode_frame(self, frame_bgr: np.ndarray) -> np.ndarray:
        """Run the EdgeSAM image encoder, returning image embeddings."""
        input_tensor = preprocess_sam(frame_bgr)

        try:
            outputs = self._sam_encoder.run(
                [self._enc_output_name],
                {self._enc_input_name: input_tensor},
            )
        except Exception as error:
            raise SegmenterError(f"EdgeSAM encoder failed: {error}") from error

        if not outputs or outputs[0] is None:
            raise SegmenterError("EdgeSAM encoder returned empty output")
        return outputs[0]

    def _decode_mask(
        self,
        embeddings: np.ndarray,
        box: tuple[float, float, float, float],
        original_size: tuple[int, int],
    ) -> tuple[tuple[float, float], ...]:
        """Run the EdgeSAM decoder for one box, returning a polygon."""
        point_coords, point_labels = box_to_sam_prompt(box, original_size, SAM_IMAGE_SIZE)

        orig_h, orig_w = original_size
        orig_size = np.array([orig_h, orig_w], dtype=np.float32)

        decoder_inputs = {
            self._dec_input_names["image_embeddings"]: embeddings,
            self._dec_input_names["point_coords"]: point_coords,
            self._dec_input_names["point_labels"]: point_labels,
            self._dec_input_names["mask_input"]: self._mask_input,
            self._dec_input_names["has_mask_input"]: self._has_mask_input,
            self._dec_input_names["orig_im_size"]: orig_size,
        }

        try:
            outputs = self._sam_decoder.run(
                [self._dec_output_name],
                decoder_inputs,
            )
        except Exception as error:
            raise SegmenterError(f"EdgeSAM decoder failed: {error}") from error

        if not outputs or outputs[0] is None:
            return ()

        mask_out: np.ndarray = outputs[0]
        mask_squeezed = mask_out.squeeze()
        if mask_squeezed.ndim != 2:
            return ()

        # Decoder outputs mask logits; convert to probabilities then binarize at 0.5
        # (not > 0.0 on logits/probs, which keeps near-zero noise as foreground).
        mask_probs = 1.0 / (1.0 + np.exp(-mask_squeezed))
        return mask_to_polygon(mask_probs > 0.5, epsilon=2.0)

    # ------------------------------------------------------------------
    # GPU pipeline setup
    # ------------------------------------------------------------------

    def _try_enable_gpu_pipeline(self) -> None:
        """Try to set up the GPU-accelerated detection pipeline."""
        if not self.device.startswith("cuda"):
            return
        try:
            from cs2_vision_access.inference.cuda.utils import cuda_available

            if not cuda_available():
                return
            from cs2_vision_access.inference.cuda.pipeline import GpuInferencePipeline

            self._gpu_pipeline = GpuInferencePipeline(
                self._detector,
                num_classes=self._det_num_classes,
                image_size=DETECTION_SIZE,
                confidence=self.confidence,
                det_input_name=self._det_input_name,
                det_output_name=self._det_output_name,
            )
            self._use_gpu_pipeline = True
            import logging

            logging.getLogger(__name__).info("GPU inference pipeline enabled for Vombit detection")
        except Exception:
            self._gpu_pipeline = None
