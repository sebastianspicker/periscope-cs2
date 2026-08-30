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

// ── RadarBlip bridge conversion ───────────────────────────────────
RadarBlipLayout blip_from_entity(float x, float y, float z, uint8_t team,
                                  bool alive, const char* label, uint32_t color,
                                  float yaw) {
    RadarBlipLayout blip{};
    // x/y are already radar-normalized screen offsets (-1..1); z is world
    // height residual retained for consumers that want elevation cues.
    blip.screen_x = x;
    blip.screen_y = y;
    (void)z;
    blip.team = team;
    blip.health = alive ? 100 : 0;
    blip.armor = 0;
    blip.is_alive = alive;
    blip.is_local = false;
    blip.is_visible = alive;
    blip.is_bomb = false;
    blip.is_hostage = false;
    blip.angle = yaw;
    blip.label[0] = '\0';
    if (label && label[0] != '\0') {
        std::strncpy(blip.label, label, sizeof(blip.label) - 1);
        blip.label[sizeof(blip.label) - 1] = '\0';
    }
    blip.color = color;
    return blip;
}

// ── Factory ───────────────────────────────────────────────────────
Result<RenderPipeline*> create_render_pipeline(int width, int height, const char* title) {
  OverlayStyle style{};
  style.width = width;
  style.height = height;
  return create_render_pipeline(style, title);
}

Result<RenderPipeline*> create_render_pipeline(const OverlayStyle& style,
                                               const char* title) {
#if LR_PLATFORM_WINDOWS
  {
    auto* d3d11 = new D3D11RenderPipeline();
    // Seed style before initialize so window creation uses alpha/topmost.
    (void)d3d11->apply_overlay_style(style);
    const Result<void> d3d_result =
        d3d11->initialize(style.width > 0 ? style.width : 320,
                          style.height > 0 ? style.height : 320, title);
    if (d3d_result.ok) {
      (void)d3d11->apply_overlay_style(style);
      return d3d11;
    }
    // Honest failure surface: never claim D3D11 when the device path failed.
    std::printf("[gpu] D3D11 unavailable (%s) — falling back to Null pipeline\n",
                d3d_result.error_msg.c_str());
    delete d3d11;
  }
#else
  std::printf("[gpu] non-Windows platform — using Null pipeline\n");
#endif
  // Null backend: api()==None, name()=="null_gpu". Callers must not treat this
  // as a successful D3D11 device; draw helpers no-op but lifecycle is valid.
  auto* null_pipeline = new NullRenderPipeline();
  const Result<void> null_result =
      null_pipeline->initialize(style.width > 0 ? style.width : 320,
                                style.height > 0 ? style.height : 320, title);
  if (null_result.ok) {
    (void)null_pipeline->apply_overlay_style(style);
    return null_pipeline;
  }
  delete null_pipeline;
  return Result<RenderPipeline*>(nullptr, "failed to initialize render pipeline");
}

Result<bool> desktop_duplication_supported() {
#if LR_PLATFORM_WINDOWS
  // Probe: factory → adapters → outputs → IDXGIOutput1 (WDDM 1.2+ / Win8+).
  IDXGIFactory1* factory = nullptr;
  HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                  reinterpret_cast<void**>(&factory));
  if (FAILED(hr) || !factory) {
    return Result<bool>(false, "CreateDXGIFactory1 failed");
  }

  bool found_output1 = false;
  real::FixedError last_err;
  for (UINT ai = 0;; ++ai) {
    IDXGIAdapter* adapter = nullptr;
    hr = factory->EnumAdapters(ai, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) break;
    if (FAILED(hr) || !adapter) {
      last_err = real::FixedError("EnumAdapters failed");
      continue;
    }

    for (UINT oi = 0;; ++oi) {
      IDXGIOutput* output = nullptr;
      hr = adapter->EnumOutputs(oi, &output);
      if (hr == DXGI_ERROR_NOT_FOUND) break;
      if (FAILED(hr) || !output) {
        last_err = real::FixedError("EnumOutputs failed");
        continue;
      }

      IDXGIOutput1* output1 = nullptr;
      hr = output->QueryInterface(__uuidof(IDXGIOutput1),
                                  reinterpret_cast<void**>(&output1));
      output->Release();
      if (SUCCEEDED(hr) && output1) {
        output1->Release();
        found_output1 = true;
        break;
      }
      last_err = real::FixedError("IDXGIOutput1 QueryInterface failed");
    }
    adapter->Release();
    if (found_output1) break;
  }
  factory->Release();

  if (found_output1) {
    return Result<bool>(true);
  }
  if (!last_err.empty()) {
    return Result<bool>(false, last_err);
  }
  return Result<bool>(false, "No DXGI output with IDXGIOutput1 support");
#else
  return Result<bool>(false, "Desktop duplication requires Windows DXGI");
#endif
}

Result<std::vector<std::uint8_t>> capture_desktop_frame(int& width, int& height) {
  width = 0;
  height = 0;
#if !LR_PLATFORM_WINDOWS
  return Result<std::vector<std::uint8_t>>(
      {}, "Desktop capture requires Windows DXGI / D3D11");
#else
  // Full educational path (not used by live radar stealth):
  // D3D11 device (BGRA) → adapter output → DuplicateOutput → AcquireNextFrame
  // → staging copy → CPU BGRA buffer.

  IDXGIFactory1* factory = nullptr;
  HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                  reinterpret_cast<void**>(&factory));
  if (FAILED(hr) || !factory) {
    return Result<std::vector<std::uint8_t>>({}, "CreateDXGIFactory1 failed");
  }

  IDXGIAdapter* adapter = nullptr;
  IDXGIOutput* output = nullptr;
  IDXGIOutput1* output1 = nullptr;

  // Prefer the first adapter/output that exposes IDXGIOutput1.
  for (UINT ai = 0; !output1; ++ai) {
    IDXGIAdapter* try_adapter = nullptr;
    hr = factory->EnumAdapters(ai, &try_adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) break;
    if (FAILED(hr) || !try_adapter) continue;

    for (UINT oi = 0; !output1; ++oi) {
      IDXGIOutput* try_output = nullptr;
      hr = try_adapter->EnumOutputs(oi, &try_output);
      if (hr == DXGI_ERROR_NOT_FOUND) break;
      if (FAILED(hr) || !try_output) continue;

      IDXGIOutput1* try_output1 = nullptr;
      hr = try_output->QueryInterface(__uuidof(IDXGIOutput1),
                                      reinterpret_cast<void**>(&try_output1));
      if (SUCCEEDED(hr) && try_output1) {
        adapter = try_adapter;
        output = try_output;
        output1 = try_output1;
        try_adapter = nullptr;
        try_output = nullptr;
        break;
      }
      try_output->Release();
    }
    if (try_adapter) try_adapter->Release();
  }
  factory->Release();
  factory = nullptr;

  if (!adapter || !output || !output1) {
    if (output1) output1->Release();
    if (output) output->Release();
    if (adapter) adapter->Release();
    return Result<std::vector<std::uint8_t>>(
        {}, "No DXGI adapter/output with IDXGIOutput1");
  }

  // Device must live on the same adapter as the duplicated output.
  ID3D11Device* device = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
  const D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
  };
  hr = D3D11CreateDevice(
      adapter,
      D3D_DRIVER_TYPE_UNKNOWN,
      nullptr,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      levels,
      static_cast<UINT>(sizeof(levels) / sizeof(levels[0])),
      D3D11_SDK_VERSION,
      &device,
      &feature_level,
      &ctx);
  // Feature-level array can fail on older runtimes; retry without it.
  if (FAILED(hr) || !device || !ctx) {
    hr = D3D11CreateDevice(
        adapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &device,
        &feature_level,
        &ctx);
  }
  adapter->Release();
  adapter = nullptr;
  if (FAILED(hr) || !device || !ctx) {
    if (output1) output1->Release();
    if (output) output->Release();
    if (device) device->Release();
    if (ctx) ctx->Release();
    return Result<std::vector<std::uint8_t>>(
        {}, "D3D11CreateDevice (BGRA) failed");
  }

  IDXGIOutputDuplication* dupe = nullptr;
  hr = output1->DuplicateOutput(device, &dupe);
  output1->Release();
  output1 = nullptr;
  output->Release();
  output = nullptr;
  if (FAILED(hr) || !dupe) {
    device->Release();
    ctx->Release();
    if (hr == E_ACCESSDENIED || hr == DXGI_ERROR_ACCESS_DENIED) {
      return Result<std::vector<std::uint8_t>>(
          {}, "DuplicateOutput access denied (secure desktop / session)");
    }
    if (hr == DXGI_ERROR_UNSUPPORTED) {
      return Result<std::vector<std::uint8_t>>(
          {}, "DuplicateOutput unsupported on this output");
    }
    if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
      return Result<std::vector<std::uint8_t>>(
          {}, "DuplicateOutput not currently available (session limit)");
    }
    return Result<std::vector<std::uint8_t>>({}, "DuplicateOutput failed");
  }

  DXGI_OUTDUPL_FRAME_INFO frame_info{};
  IDXGIResource* desktop_res = nullptr;
  constexpr UINT kAcquireTimeoutMs = 750;
  hr = dupe->AcquireNextFrame(kAcquireTimeoutMs, &frame_info, &desktop_res);
  if (FAILED(hr) || !desktop_res) {
    dupe->Release();
    device->Release();
    ctx->Release();
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
      return Result<std::vector<std::uint8_t>>(
          {}, "AcquireNextFrame timed out (no desktop update)");
    }
    if (hr == DXGI_ERROR_ACCESS_LOST) {
      return Result<std::vector<std::uint8_t>>(
          {}, "AcquireNextFrame access lost (mode change / UAC)");
    }
    return Result<std::vector<std::uint8_t>>({}, "AcquireNextFrame failed");
  }

  ID3D11Texture2D* desktop_tex = nullptr;
  hr = desktop_res->QueryInterface(__uuidof(ID3D11Texture2D),
                                   reinterpret_cast<void**>(&desktop_tex));
  desktop_res->Release();
  desktop_res = nullptr;
  if (FAILED(hr) || !desktop_tex) {
    dupe->ReleaseFrame();
    dupe->Release();
    device->Release();
    ctx->Release();
    return Result<std::vector<std::uint8_t>>(
        {}, "Desktop resource is not ID3D11Texture2D");
  }

  D3D11_TEXTURE2D_DESC desc{};
  desktop_tex->GetDesc(&desc);
  if (desc.Width == 0 || desc.Height == 0) {
    desktop_tex->Release();
    dupe->ReleaseFrame();
    dupe->Release();
    device->Release();
    ctx->Release();
    return Result<std::vector<std::uint8_t>>({}, "Desktop texture has zero size");
  }

  D3D11_TEXTURE2D_DESC staging_desc = desc;
  staging_desc.BindFlags = 0;
  staging_desc.MiscFlags = 0;
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  staging_desc.Usage = D3D11_USAGE_STAGING;
  staging_desc.ArraySize = 1;
  staging_desc.MipLevels = 1;
  staging_desc.SampleDesc.Count = 1;
  staging_desc.SampleDesc.Quality = 0;

  ID3D11Texture2D* staging = nullptr;
  hr = device->CreateTexture2D(&staging_desc, nullptr, &staging);
  if (FAILED(hr) || !staging) {
    desktop_tex->Release();
    dupe->ReleaseFrame();
    dupe->Release();
    device->Release();
    ctx->Release();
    return Result<std::vector<std::uint8_t>>({}, "CreateTexture2D staging failed");
  }

  ctx->CopyResource(staging, desktop_tex);
  desktop_tex->Release();
  desktop_tex = nullptr;

  D3D11_MAPPED_SUBRESOURCE mapped{};
  hr = ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
  if (FAILED(hr) || !mapped.pData) {
    staging->Release();
    dupe->ReleaseFrame();
    dupe->Release();
    device->Release();
    ctx->Release();
    return Result<std::vector<std::uint8_t>>({}, "Map staging texture failed");
  }

  const int w = static_cast<int>(desc.Width);
  const int h = static_cast<int>(desc.Height);
  const std::size_t row_bytes =
      static_cast<std::size_t>(w) * 4u;  // BGRA8
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) *
                                   static_cast<std::size_t>(h) * 4u);
  const auto* src_base = static_cast<const std::uint8_t*>(mapped.pData);
  for (int y = 0; y < h; ++y) {
    const std::uint8_t* src_row =
        src_base + static_cast<std::size_t>(y) * mapped.RowPitch;
    std::uint8_t* dst_row = pixels.data() + static_cast<std::size_t>(y) * row_bytes;
    std::memcpy(dst_row, src_row, row_bytes);
  }

  ctx->Unmap(staging, 0);
  staging->Release();
  dupe->ReleaseFrame();
  dupe->Release();
  ctx->Release();
  device->Release();

  width = w;
  height = h;
  return Result<std::vector<std::uint8_t>>(std::move(pixels));
#endif
}

}  // namespace real::gpu
