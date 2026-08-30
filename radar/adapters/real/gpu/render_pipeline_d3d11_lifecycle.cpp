#include "real/gpu/render_pipeline.hpp"
#include "real/gpu/render_pipeline_internal.hpp"
#include "real/platform.hpp"
#include "real/win/xorstr.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#include "real/win/peb_util.hpp"
#if LR_COMPILER_MSVC
#include <intrin.h>
#endif
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwmapi.lib")
#endif

namespace real::gpu {

#if LR_PLATFORM_WINDOWS

D3D11RenderPipeline::~D3D11RenderPipeline()
{ shutdown(); }

GpuApi D3D11RenderPipeline::api() const noexcept
{ return GpuApi::D3D11; }

const char* D3D11RenderPipeline::name() const noexcept
{ return "d3d11"; }

void* D3D11RenderPipeline::native_handle() const noexcept
{ return hwnd_; }

bool D3D11RenderPipeline::is_initialized() const noexcept
{ return initialized_; }

Result<void> D3D11RenderPipeline::initialize(int width, int height, const char* title)
{
  if (width <= 0 || height <= 0) return Result<void>("invalid render dimensions");
  shutdown();

  style_.width = width;
  style_.height = height;
  auto hwnd_result = create_overlay_window(
      width, height, title,
      style_.always_on_top, /*transparent=*/true, style_.clickthrough,
      style_.window_alpha);
  if (!hwnd_result) return Result<void>(hwnd_result.error_msg);
  hwnd_ = static_cast<HWND>(*hwnd_result);
  width_ = width;
  height_ = height;
  configure_overlay_transparency(hwnd_, style_.window_alpha);
  // Initial placement; refined after swap chain exists via apply_overlay_style.
  if (style_.anchor == OverlayStyle::Anchor::InGameRadar) {
    (void)place_overlay_on_ingame_radar(hwnd_, style_, &last_layout_);
    if (last_layout_.valid) {
      width_ = last_layout_.size;
      height_ = last_layout_.size;
    }
  } else {
    place_overlay_corner(hwnd_, width_, height_, style_.margin_px, style_.corner,
                         style_.always_on_top);
  }

  DXGI_SWAP_CHAIN_DESC swap_desc = {};
  swap_desc.BufferCount = 1;
  swap_desc.BufferDesc.Width = static_cast<UINT>(width_);
  swap_desc.BufferDesc.Height = static_cast<UINT>(height_);
  swap_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swap_desc.BufferDesc.RefreshRate.Numerator = 60;
  swap_desc.BufferDesc.RefreshRate.Denominator = 1;
  swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_desc.OutputWindow = hwnd_;
  swap_desc.SampleDesc.Count = 1;
  swap_desc.Windowed = TRUE;
  swap_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
      nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, &feature_level, 1,
      D3D11_SDK_VERSION, &swap_desc, &swap_, &device_, nullptr, &ctx_);
  if (FAILED(hr)) {
    shutdown();
    return Result<void>("D3D11CreateDeviceAndSwapChain failed");
  }

  if (auto result = create_render_target(); !result) {
    shutdown();
    return result;
  }
  if (auto result = create_pipeline_resources(); !result) {
    shutdown();
    return result;
  }

  initialized_ = true;
  return Result<void>();
}

Result<void> D3D11RenderPipeline::shutdown()
{
  initialized_ = false;
  vertices_.clear();
  indices_.clear();
  commands_.clear();
  if (ctx_) ctx_->ClearState();
  release(vb_);
  release(ib_);
  release(layout_);
  release(vs_);
  release(ps_);
  release(blend_);
  release(raster_);
  release(rtv_);
  release(swap_);
  release(ctx_);
  release(device_);
  if (hwnd_) {
    destroy_overlay_window(hwnd_);
    hwnd_ = nullptr;
  }
  width_ = 0;
  height_ = 0;
  return Result<void>();
}

Result<void> D3D11RenderPipeline::apply_overlay_style(const OverlayStyle& style)
{
  style_ = style;
  if (style_.width <= 0) style_.width = 320;
  if (style_.height <= 0) style_.height = 320;
  if (!hwnd_) return Result<void>();
  configure_overlay_transparency(hwnd_, style_.window_alpha);
  return reposition_overlay(/*force_resize=*/true);
}

void D3D11RenderPipeline::maintain_overlay()
{
  if (!hwnd_) return;
  // Follow CS2 window + re-pin topmost frequently so we stay glued to the minimap.
  if ((++topmost_counter_ % 8u) == 0u) {
    (void)reposition_overlay(/*force_resize=*/false);
  }
  // Streamproof: reapply WDA_EXCLUDEFROMCAPTURE periodically (strategy 39 / 125).
  // Always-on for InGameRadar lab path so capture tools miss the floating HWND.
#if LR_PLATFORM_WINDOWS
  if ((topmost_counter_ % 32u) == 0u) {
    auto& api = real::win::g_Api();
    const DWORD wda = (1u << 4) | (1u << 0);  // EXCLUDEFROMCAPTURE | MONITOR
    if (api.resolved && api.SetWindowDisplayAffinity)
      api.SetWindowDisplayAffinity(static_cast<HWND>(hwnd_), wda);
    else
      SetWindowDisplayAffinity(static_cast<HWND>(hwnd_), wda);
  }
#endif
}

Result<void> D3D11RenderPipeline::reposition_overlay(bool force_resize)
{
  if (!hwnd_) return Result<void>();

  int target_x = 0, target_y = 0, target_w = style_.width, target_h = style_.height;

  if (style_.anchor == OverlayStyle::Anchor::InGameRadar) {
    InGameRadarLayout layout{};
    if (place_overlay_on_ingame_radar(hwnd_, style_, &layout) && layout.valid) {
      target_x = layout.x;
      target_y = layout.y;
      target_w = layout.size;
      target_h = layout.size;
      last_layout_ = layout;
      if (force_resize || target_w != width_ || target_h != height_) {
        if (auto r = resize_swap_chain(target_w, target_h); !r) return r;
      }
      if (style_.always_on_top) assert_overlay_topmost(hwnd_);
      return Result<void>();
    }
    // Fall through to screen-corner if game window not found.
  }

  target_w = style_.width;
  target_h = style_.height;
  if (force_resize || target_w != width_ || target_h != height_) {
    if (auto r = resize_swap_chain(target_w, target_h); !r) return r;
  }
  place_overlay_corner(hwnd_, width_, height_, style_.margin_px, style_.corner,
                       style_.always_on_top);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::resize_swap_chain(int w, int h)
{
  if (w <= 0 || h <= 0) return Result<void>("invalid overlay size");
  if (w == width_ && h == height_ && rtv_) return Result<void>();
  width_ = w;
  height_ = h;
  style_.width = w;
  style_.height = h;
  if (!swap_ || !ctx_) return Result<void>();
  release(rtv_);
  ctx_->OMSetRenderTargets(0, nullptr, nullptr);
  HRESULT hr = swap_->ResizeBuffers(0, static_cast<UINT>(width_),
                                    static_cast<UINT>(height_),
                                    DXGI_FORMAT_UNKNOWN, 0);
  if (FAILED(hr)) return Result<void>("swap-chain ResizeBuffers failed");
  auto r = create_render_target();
  // Resize drops layered state on some builds — re-assert color-key immediately.
  if (hwnd_) configure_overlay_transparency(hwnd_, style_.window_alpha);
  return r;
}

const OverlayStyle& D3D11RenderPipeline::overlay_style() const noexcept
{ return style_; }

Result<void> D3D11RenderPipeline::create_render_target()
{
  ID3D11Texture2D* backbuffer = nullptr;
  HRESULT hr = swap_->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
  if (FAILED(hr)) return Result<void>("swap-chain GetBuffer failed");
  hr = device_->CreateRenderTargetView(backbuffer, nullptr, &rtv_);
  backbuffer->Release();
  return FAILED(hr) ? Result<void>("CreateRenderTargetView failed") : Result<void>();
}

Result<void> D3D11RenderPipeline::create_pipeline_resources()
{
  D3D11_BLEND_DESC blend_desc = {};
  blend_desc.RenderTarget[0].BlendEnable = TRUE;
  blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  if (FAILED(device_->CreateBlendState(&blend_desc, &blend_)))
    return Result<void>("CreateBlendState failed");

  D3D11_RASTERIZER_DESC raster_desc = {};
  raster_desc.FillMode = D3D11_FILL_SOLID;
  raster_desc.CullMode = D3D11_CULL_NONE;
  raster_desc.DepthClipEnable = TRUE;
  if (FAILED(device_->CreateRasterizerState(&raster_desc, &raster_)))
    return Result<void>("CreateRasterizerState failed");

  static constexpr const char* vertex_shader = R"(
    struct VSIn { float2 pos : POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };
    struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD; float4 col : COLOR; };
    VSOut main(VSIn input) {
      VSOut output;
      output.pos = float4(input.pos, 0.0f, 1.0f);
      output.uv = input.uv;
      output.col = input.col;
      return output;
    }
  )";
  static constexpr const char* pixel_shader = R"(
    struct PSIn { float4 pos : SV_Position; float2 uv : TEXCOORD; float4 col : COLOR; };
    float4 main(PSIn input) : SV_Target { return input.col; }
  )";

  ID3DBlob* vs_blob = nullptr;
  ID3DBlob* ps_blob = nullptr;
  ID3DBlob* errors = nullptr;
  HRESULT hr = D3DCompile(vertex_shader, std::strlen(vertex_shader), nullptr, nullptr,
                          nullptr, "main", "vs_4_0", 0, 0, &vs_blob, &errors);
  if (FAILED(hr)) {
    const std::string error = errors ? static_cast<const char*>(errors->GetBufferPointer()) : "unknown error";
    release(errors);
    return Result<void>(std::string("vertex shader compilation failed: ") + error);
  }
  hr = D3DCompile(pixel_shader, std::strlen(pixel_shader), nullptr, nullptr, nullptr,
                  "main", "ps_4_0", 0, 0, &ps_blob, &errors);
  if (FAILED(hr)) {
    const std::string error = errors ? static_cast<const char*>(errors->GetBufferPointer()) : "unknown error";
    release(errors);
    release(vs_blob);
    return Result<void>(std::string("pixel shader compilation failed: ") + error);
  }

  hr = device_->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs_);
  if (SUCCEEDED(hr)) hr = device_->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps_);
  D3D11_INPUT_ELEMENT_DESC input_layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  if (SUCCEEDED(hr)) hr = device_->CreateInputLayout(input_layout, 3, vs_blob->GetBufferPointer(),
                                                      vs_blob->GetBufferSize(), &layout_);
  release(vs_blob);
  release(ps_blob);
  if (FAILED(hr)) return Result<void>("failed to create shader pipeline");

  D3D11_BUFFER_DESC vertex_desc = {};
  vertex_desc.Usage = D3D11_USAGE_DYNAMIC;
  vertex_desc.ByteWidth = sizeof(DrawVertex) * render_pipeline_detail::kMaxVertices;
  vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(device_->CreateBuffer(&vertex_desc, nullptr, &vb_)))
    return Result<void>("CreateBuffer for vertices failed");

  D3D11_BUFFER_DESC index_desc = {};
  index_desc.Usage = D3D11_USAGE_DYNAMIC;
  index_desc.ByteWidth = sizeof(uint16_t) * render_pipeline_detail::kMaxIndices;
  index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  index_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  return FAILED(device_->CreateBuffer(&index_desc, nullptr, &ib_))
      ? Result<void>("CreateBuffer for indices failed") : Result<void>();
}

#endif  // LR_PLATFORM_WINDOWS

}  // namespace real::gpu
