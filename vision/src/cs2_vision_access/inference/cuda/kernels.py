# CUDA kernel sources — compiled at runtime via CuPy RawKernel.
#
# Each kernel is a C++ string constant that gets JIT-compiled by nvrtc
# (NVIDIA Runtime Compilation library) on first use.  No separate .cu
# compilation step is needed.
#
# Prerequisites:
#   - NVIDIA GPU with compute capability 5.0+
#   - CUDA Toolkit 11.x+ (for nvrtc)
#   - CuPy 12.x+ (pip install cupy-cuda12x)
#

from __future__ import annotations

# =========================================================================
# NMS — Non-Maximum Suppression
#
# Each thread handles box i.  It iterates over all higher-scoring boxes j
# (j < i after score-sorted order).  If any kept box j has IoU(box_i, box_j)
# > threshold, box i is marked suppressed.
# =========================================================================

NMS_KERNEL_CU = """
extern "C" {

__device__ float bbox_iou(
    const float* a, const float* b
) {
    float inter_x1 = max(a[0], b[0]);
    float inter_y1 = max(a[1], b[1]);
    float inter_x2 = min(a[2], b[2]);
    float inter_y2 = min(a[3], b[3]);

    float inter_w = max(0.0f, inter_x2 - inter_x1);
    float inter_h = max(0.0f, inter_y2 - inter_y1);
    float inter_area = inter_w * inter_h;

    float area_a = (a[2] - a[0]) * (a[3] - a[1]);
    float area_b = (b[2] - b[0]) * (b[3] - b[1]);
    float union_area = area_a + area_b - inter_area;

    return union_area > 0.0f ? inter_area / union_area : 0.0f;
}

__global__ void nms_kernel(
    const float* boxes,         // [N, 4] xyxy, sorted by score descending
    const int* indices,         // [N] original indices (sorted by score)
    bool* suppressed,           // [N] output — true = suppress this box
    int N,
    float iou_threshold
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) return;

    const float* box_i = &boxes[i * 4];

    for (int j = 0; j < i; ++j) {
        if (suppressed[j]) continue;
        const float* box_j = &boxes[j * 4];
        float iou = bbox_iou(box_i, box_j);
        if (iou > iou_threshold) {
            suppressed[i] = true;
            return;
        }
    }
    suppressed[i] = false;
}

}  // extern "C"
"""


# =========================================================================
# Letterbox pre-processing
#
# Converts a BGR uint8 frame to a float32 CHW tensor in [0, 1] range,
# RGB channel order, with letterbox padding.  Each thread processes one
# output pixel (C, y, x).
# =========================================================================

LETTERBOX_KERNEL_CU = """
extern "C" {

__device__ unsigned char sample_bilinear(
    const unsigned char* src, int src_w, int src_h, int stride,
    float x, float y, int channel
) {
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    int x1 = min(x0 + 1, src_w - 1);
    int y1 = min(y0 + 1, src_h - 1);

    float fx = x - x0;
    float fy = y - y0;

    float v00 = (float)src[y0 * stride + x0 * 3 + channel];
    float v10 = (float)src[y0 * stride + x1 * 3 + channel];
    float v01 = (float)src[y1 * stride + x0 * 3 + channel];
    float v11 = (float)src[y1 * stride + x1 * 3 + channel];

    float row0 = v00 + (v10 - v00) * fx;
    float row1 = v01 + (v11 - v01) * fx;
    return (unsigned char)(row0 + (row1 - row0) * fy);
}

__global__ void letterbox_kernel(
    const unsigned char* src,   // [H, W, 3] BGR uint8
    int src_w, int src_h,
    int src_stride,
    float* dst,                 // [3, dst_h, dst_w] RGB float32
    int dst_w, int dst_h,
    float inv_scale_x,          // src_w / new_w (scale factor to original)
    float inv_scale_y,
    int pad_left,
    int pad_top
) {
    // 3D grid: (c, y, x)
    int c = blockIdx.z;
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (c >= 3 || x >= dst_w || y >= dst_h) return;

    // Map output pixel to source pixel (inverse of letterbox)
    float src_x = (x - pad_left) * inv_scale_x;
    float src_y = (y - pad_top) * inv_scale_y;

    // Padded area → black
    if (src_x < 0 || src_x >= src_w || src_y < 0 || src_y >= src_h) {
        dst[c * dst_h * dst_w + y * dst_w + x] = 0.0f;
        return;
    }

    // Sample source (BGR) and reorder to RGB
    int src_c;
    if (c == 0) src_c = 2;  // R ← B (index 2)
    else if (c == 1) src_c = 1;  // G ← G
    else src_c = 0;  // B ← R (index 0)

    unsigned char pixel = sample_bilinear(src, src_w, src_h, src_stride, src_x, src_y, src_c);
    dst[c * dst_h * dst_w + y * dst_w + x] = (float)pixel / 255.0f;
}

}  // extern "C"
"""


# =========================================================================
# YOLO box decode
#
# Converts the raw Ultralytics ONNX output tensor into xyxy boxes + scores
# + class IDs.  Each thread decodes one prediction (8400 total for
# 640×640 input).
#
# Raw layout:  [4 + num_classes, 8400]
#   rows 0-3:     cx, cy, w, h  (grid-scaled, 0-640)
#   rows 4-83:    class scores
# =========================================================================

YOLO_DECODE_KERNEL_CU = """
extern "C" {

__global__ void yolo_decode_kernel(
    const float* raw,           // [4 + num_classes, num_predictions]
    int num_classes,
    int num_predictions,
    float scale,                // DETECTION_SIZE (640)
    float orig_w, float orig_h,
    float* boxes,               // [num_predictions, 4] xyxy output
    float* scores,              // [num_predictions] output
    int* class_ids              // [num_predictions] output
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_predictions) return;

    // Stride: raw tensor is column-major from ONNX perspective
    // raw[c, i] = raw[c * num_predictions + i]
    #define RAW(c) raw[(c) * num_predictions + i]

    float cx = RAW(0) / scale * orig_w;
    float cy = RAW(1) / scale * orig_h;
    float w  = RAW(2) / scale * orig_w;
    float h  = RAW(3) / scale * orig_h;

    boxes[i * 4 + 0] = fmaxf(0.0f, cx - w * 0.5f);
    boxes[i * 4 + 1] = fmaxf(0.0f, cy - h * 0.5f);
    boxes[i * 4 + 2] = fminf(orig_w, cx + w * 0.5f);
    boxes[i * 4 + 3] = fminf(orig_h, cy + h * 0.5f);

    // Argmax over classes
    int best_cls = 0;
    float best_score = RAW(4);
    for (int c = 1; c < num_classes; ++c) {
        float s = RAW(4 + c);
        if (s > best_score) {
            best_score = s;
            best_cls = c;
        }
    }

    scores[i] = best_score;
    class_ids[i] = best_cls;

    #undef RAW
}

}  // extern "C"
"""


# =========================================================================
# EdgeSAM preprocessor — ImageNet normalisation
#
# Takes the RGB float32 [0,1] tensor from letterbox and applies
# per-channel ImageNet normalisation:
#
#   output[c] = (input[c] - mean[c]) / std[c]
# =========================================================================

IMAGENET_NORM_KERNEL_CU = """
extern "C" {

__global__ void imagenet_norm_kernel(
    float* tensor,      // [3, H, W] RGB float32, modified in-place
    int H, int W,
    const float* mean,  // [3] — e.g. {0.485, 0.456, 0.406}
    const float* std    // [3] — e.g. {0.229, 0.224, 0.225}
) {
    int c = blockIdx.z;
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (c >= 3 || x >= W || y >= H) return;

    int idx = c * H * W + y * W + x;
    tensor[idx] = (tensor[idx] - mean[c]) / std[c];
}

}  // extern "C"
"""


# =========================================================================
# Mask marching-squares contour extraction
#
# Converts a binary mask probability map into a set of contour vertices
# using a GPU-parallel marching squares algorithm.
#
# Each thread block processes a 16×16 tile of the mask.  Threads within
# the block examine 4 neighbouring pixels to classify the cell type and
# emit line segments for the contour.
#
# Reference: https://en.wikipedia.org/wiki/Marching_squares
# =========================================================================

MARCHING_SQUARES_CU = """
extern "C" {

#define EMPTY  (-1)

__device__ int emit_contour_segment(
    float* contour_buf,
    int* contour_count,
    int max_vertices,
    float x1, float y1, float x2, float y2
) {
    int idx = atomicAdd(contour_count, 2);
    if (idx + 2 > max_vertices) {
        atomicAdd(contour_count, -2);
        return -1;
    }
    contour_buf[idx * 2 + 0] = x1;
    contour_buf[idx * 2 + 1] = y1;
    contour_buf[idx * 2 + 2] = x2;
    contour_buf[idx * 2 + 3] = y2;
    return 0;
}

__global__ void marching_squares_kernel(
    const float* mask,          // [H, W] mask logits (sigmoid applied externally)
    int H, int W,
    float threshold,
    float* contour_vertices,    // output [max_vertices, 2] (x, y)
    int* contour_counts,        // output [1] number of vertices written
    int max_vertices
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= W - 1 || y >= H - 1) return;

    // Sample 4 corners of the cell (x, y) → (x+1, y+1)
    float tl = mask[y * W + x];
    float tr = mask[y * W + x + 1];
    float bl = mask[(y + 1) * W + x];
    float br = mask[(y + 1) * W + x + 1];

    // Classify cell (4-bit code)
    int code = 0;
    if (tl > threshold) code |= 1;
    if (tr > threshold) code |= 2;
    if (bl > threshold) code |= 4;
    if (br > threshold) code |= 8;

    if (code == 0 || code == 15) return;  // entirely empty or full

    float cx = x + 0.5f;
    float cy = y + 0.5f;

    // Emit line segment(s) based on cell type
    // Edge midpoints: top=(x+0.5, y), right=(x+1, y+0.5),
    //                bottom=(x+0.5, y+1), left=(x, y+0.5)
    switch (code) {
        case 1:  // tl only
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y, x, cy);
            break;
        case 2:  // tr only
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, x+1, cy, cx, y);
            break;
        case 3:  // tl + tr (top edge)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, x, cy, x+1, cy);
            break;
        case 4:  // bl only
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, x, cy, cx, y+1);
            break;
        case 5:  // tl + bl (left edge)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y, cx, y+1);
            break;
        case 6:  // tr + bl (saddle)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, x, cy, cx, y);
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y+1, x+1, cy);
            break;
        case 7:  // tl + tr + bl (inverted 8)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, x, cy, cx, y+1);
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y+1, x+1, cy);
            break;
        case 8:  // br only
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y+1, x+1, cy);
            break;
        case 9:  // tl + br (saddle)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y, x+1, cy);
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, x, cy, cx, y+1);
            break;
        case 10: // tr + br (right edge)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y, cx, y+1);
            break;
        case 11: // tl + tr + br (inverted 4)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, x, cy, cx, y);
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y, cx, y+1);
            break;
        case 12: // bl + br (bottom edge)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, x, cy, x+1, cy);
            break;
        case 13: // tl + bl + br (inverted 2)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y, x+1, cy);
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, x, cy, cx, y);
            break;
        case 14: // tr + bl + br (inverted 1)
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y, x+1, cy);
            emit_contour_segment(contour_vertices, contour_counts, max_vertices, cx, y+1, x+1, cy);
            break;
    }
}

}  // extern "C"
"""


__all__ = [
    "IMAGENET_NORM_KERNEL_CU",
    "LETTERBOX_KERNEL_CU",
    "MARCHING_SQUARES_CU",
    "NMS_KERNEL_CU",
    "YOLO_DECODE_KERNEL_CU",
]
