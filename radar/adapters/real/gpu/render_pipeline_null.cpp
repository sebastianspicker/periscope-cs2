#include "real/gpu/render_pipeline.hpp"
#include "real/gpu/render_pipeline_internal.hpp"

// NullRenderPipeline is fully defined in render_pipeline_internal.hpp
// (small honest fallback; kept header-visible for the factory).

namespace real::gpu {
namespace {
// TU anchor so the null backend remains an explicit build unit.
volatile int g_null_render_pipeline_tu = 0;
}
}  // namespace real::gpu
