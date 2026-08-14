# GPU extras

Optional dependency groups in `pyproject.toml`:

| Extra | Contents |
|-------|----------|
| `gpu` | `onnxruntime-gpu`, `onnx`, `onnxconverter-common`, `cupy-cuda12x` |
| `gpu-cuda11` | `cupy-cuda11x` |
| `gpu-cuda-local` | source `cupy` build |

```bash
uv sync --extra gpu
```

## Providers

ONNX Runtime providers depend on the installed wheel and drivers:

```bash
uv run python -c "import onnxruntime as ort; print(ort.get_available_providers())"
```

When CUDA works you typically see `CUDAExecutionProvider`. DirectML may appear on Windows with the GPU ORT build. TensorRT needs NVIDIA tooling outside this package.

## CuPy

CUDA kernels under `inference/cuda/` use CuPy when available. Match the wheel to your CUDA major version (`cupy-cuda12x` vs `cupy-cuda11x`). If import fails, implemented paths fall back to CPU.

## CLI device flags

Many commands accept `--device` (`cpu`, `cuda:0`, …). Live, outline, train, bakeoff, and export-predictions all take it where inference runs. Cloud train helpers auto-detect CUDA when torch is installed.

## Verification

```bash
nvidia-smi
uv run python -c "import torch; print(torch.cuda.is_available())"  # if torch installed
uv run cs2-vision benchmark --input path/to/clip.mp4 --model ... --manifest ... --device cuda:0
```

Measure latency on your hardware; blog numbers are not guarantees.
