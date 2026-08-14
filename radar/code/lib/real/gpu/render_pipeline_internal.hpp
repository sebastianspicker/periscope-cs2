#pragma once

#include "real/gpu/render_pipeline.hpp"
#include "real/win/xorstr.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::gpu {

// Null GPU backend (honest fallback when D3D11 is unavailable).
class NullRenderPipeline final : public RenderPipeline {
public:
  GpuApi api() const noexcept override { return GpuApi::None; }
  const char* name() const noexcept override { return "null_gpu"; }

  Result<void> initialize(int width, int height, const char* title) override {
    width_ = width;
    height_ = height;
    title_ = title ? title : OBF("SvcHost");
    initialized_ = true;
    return Result<void>();
  }

  Result<void> shutdown() override {
    initialized_ = false;
    return Result<void>();
  }

  bool is_initialized() const noexcept override { return initialized_; }
  void* native_handle() const noexcept override { return nullptr; }
  Result<void> apply_overlay_style(const OverlayStyle& style) override {
    style_ = style;
    width_ = style.width;
    height_ = style.height;
    return Result<void>();
  }
  void maintain_overlay() override {}
  const OverlayStyle& overlay_style() const noexcept override { return style_; }
  Result<void> begin_frame() override { return initialized_ ? Result<void>() : Result<void>("not initialized"); }
  Result<void> end_frame() override { return initialized_ ? Result<void>() : Result<void>("not initialized"); }
  Result<void> draw_radar_background(float, float, float, uint32_t, uint32_t) override { return Result<void>(); }
  Result<void> draw_player_dot(float, float, float, uint32_t) override { return Result<void>(); }
  Result<void> draw_player_arrow(float, float, float, float, uint32_t) override { return Result<void>(); }
  Result<void> draw_health_bar(float, float, float, float, int) override { return Result<void>(); }
  Result<void> draw_health_ring(float, float, float, int, int) override { return Result<void>(); }
  Result<void> draw_text(float, float, const char*, uint32_t, float) override { return Result<void>(); }
  Result<void> draw_line(float, float, float, float, uint32_t) override { return Result<void>(); }
  Result<void> draw_rect_filled(float, float, float, float, uint32_t) override { return Result<void>(); }
  Result<void> draw_rect(float, float, float, float, uint32_t, float) override { return Result<void>(); }
  Result<void> draw_crosshair(float, float, float, uint32_t) override { return Result<void>(); }
  Result<void> draw_radar_frame(const RadarFrame&) override { return Result<void>(); }
  bool process_messages() override { return true; }

private:
  bool initialized_ = false;
  int width_ = 0;
  int height_ = 0;
  std::string title_;
  OverlayStyle style_{};
};


}  // namespace real::gpu

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include <d3d11.h>

namespace real::gpu {

namespace render_pipeline_detail {
constexpr uint32_t kMaxVertices = 16384;
constexpr uint32_t kMaxIndices = 32768;
constexpr float kPi = 3.14159265358979323846f;
}  // namespace render_pipeline_detail

// D3D11 implementation (method bodies live in render_pipeline_d3d11_*.cpp).
class D3D11RenderPipeline final : public RenderPipeline {
public:
  ~D3D11RenderPipeline() override;
  GpuApi api() const noexcept override;
  const char* name() const noexcept override;
  void* native_handle() const noexcept override;
  bool is_initialized() const noexcept override;
  Result<void> initialize(int width, int height, const char* title) override;
  Result<void> shutdown() override;
  Result<void> apply_overlay_style(const OverlayStyle& style) override;
  void maintain_overlay() override;
  Result<void> reposition_overlay(bool force_resize);
  Result<void> resize_swap_chain(int w, int h);
  const OverlayStyle& overlay_style() const noexcept override;
  Result<void> begin_frame() override;
  Result<void> end_frame() override;
  Result<void> draw_radar_background(float cx, float cy, float radius,
                                     uint32_t bg_color, uint32_t border_color) override;
  Result<void> draw_player_dot(float x, float y, float radius, uint32_t color) override;
  Result<void> draw_player_arrow(float x, float y, float angle, float size,
                                 uint32_t color) override;
  Result<void> draw_health_bar(float x, float y, float width, float height,
                               int health) override;
  Result<void> draw_health_ring(float x, float y, float radius, int health,
                                int armor) override;
  Result<void> draw_text(float x, float y, const char* text, uint32_t color,
                         float size) override;
  Result<void> draw_line(float x1, float y1, float x2, float y2, uint32_t color) override;
  Result<void> draw_rect_filled(float x, float y, float w, float h,
                                uint32_t color) override;
  Result<void> draw_rect(float x, float y, float w, float h, uint32_t color,
                         float thickness) override;
  Result<void> draw_crosshair(float x, float y, float size, uint32_t color) override;
  Result<void> draw_radar_frame(const RadarFrame& frame) override;
  bool process_messages() override;
private:
  template <typename T>
  static void release(T*& resource) {
    if (resource) {
      resource->Release();
      resource = nullptr;
    }
  }
  Result<void> create_render_target();
  Result<void> create_pipeline_resources();
  void add_command(DrawCommand::Type type, uint32_t vertex_offset, uint32_t vertex_count,
                   uint32_t index_offset, uint32_t index_count);
  void push_quad(float x1, float y1, float x2, float y2, uint32_t color);
  void push_ring(float cx, float cy, float radius, uint32_t color, int segments);
  void push_circle(float cx, float cy, float radius, uint32_t color, int segments);
  void push_line(float x1, float y1, float x2, float y2, uint32_t color);
  void push_triangle(float x1, float y1, float x2, float y2, float x3, float y3,
                     uint32_t color);
  HWND hwnd_ = nullptr;
  ID3D11Device* device_ = nullptr;
  ID3D11DeviceContext* ctx_ = nullptr;
  IDXGISwapChain* swap_ = nullptr;
  ID3D11RenderTargetView* rtv_ = nullptr;
  ID3D11VertexShader* vs_ = nullptr;
  ID3D11PixelShader* ps_ = nullptr;
  ID3D11InputLayout* layout_ = nullptr;
  ID3D11Buffer* vb_ = nullptr;
  ID3D11Buffer* ib_ = nullptr;
  ID3D11BlendState* blend_ = nullptr;
  ID3D11RasterizerState* raster_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  bool initialized_ = false;
  OverlayStyle style_{};
  InGameRadarLayout last_layout_{};
  std::uint32_t topmost_counter_ = 0;
  std::vector<DrawVertex> vertices_;
  std::vector<uint16_t> indices_;
  std::vector<DrawCommand> commands_;
};

}  // namespace real::gpu

#endif  // LR_PLATFORM_WINDOWS

