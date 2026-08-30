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

Result<void> D3D11RenderPipeline::begin_frame()
{
  if (!initialized_ || !ctx_ || !rtv_) return Result<void>("not initialized");
  maintain_overlay();
  vertices_.clear();
  indices_.clear();
  commands_.clear();
  // Clear to chroma-key RGB(16,0,16) so color-key punches full transparency.
  // Avoid pure black: DWM+layered windows sometimes composite it as an opaque plate.
  const float clear_color[4] = {16.0f / 255.0f, 0.0f, 16.0f / 255.0f, 1.0f};
  ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
  ctx_->ClearRenderTargetView(rtv_, clear_color);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::end_frame()
{
  if (!initialized_ || !ctx_ || !swap_ || !rtv_) return Result<void>("not initialized");
  if (!commands_.empty()) {
    if (!vb_ || !ib_ || !vs_ || !ps_ || !layout_) {
      return Result<void>("pipeline resources missing");
    }
    if (vertices_.size() > render_pipeline_detail::kMaxVertices || indices_.size() > render_pipeline_detail::kMaxIndices)
      return Result<void>("radar frame exceeds dynamic buffer capacity");

    D3D11_MAPPED_SUBRESOURCE vertex_map = {};
    HRESULT hr = ctx_->Map(vb_, 0, D3D11_MAP_WRITE_DISCARD, 0, &vertex_map);
    if (FAILED(hr) || !vertex_map.pData) return Result<void>("failed to map vertex buffer");
    std::memcpy(vertex_map.pData, vertices_.data(), vertices_.size() * sizeof(DrawVertex));
    ctx_->Unmap(vb_, 0);

    D3D11_MAPPED_SUBRESOURCE index_map = {};
    hr = ctx_->Map(ib_, 0, D3D11_MAP_WRITE_DISCARD, 0, &index_map);
    if (FAILED(hr) || !index_map.pData) return Result<void>("failed to map index buffer");
    std::memcpy(index_map.pData, indices_.data(), indices_.size() * sizeof(uint16_t));
    ctx_->Unmap(ib_, 0);

    UINT stride = sizeof(DrawVertex);
    UINT offset = 0;
    ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
    ctx_->IASetVertexBuffers(0, 1, &vb_, &stride, &offset);
    ctx_->IASetIndexBuffer(ib_, DXGI_FORMAT_R16_UINT, 0);
    ctx_->IASetInputLayout(layout_);
    ctx_->VSSetShader(vs_, nullptr, 0);
    ctx_->PSSetShader(ps_, nullptr, 0);
    if (blend_) ctx_->OMSetBlendState(blend_, nullptr, 0xFFFFFFFF);
    if (raster_) ctx_->RSSetState(raster_);
    const D3D11_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(width_),
        static_cast<float>(height_), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &viewport);

    for (const DrawCommand& command : commands_) {
      if (command.index_count == 0) continue;
      ctx_->IASetPrimitiveTopology(command.type == DrawCommand::TriangleList
          ? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
          : command.type == DrawCommand::LineStrip
              ? D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
              : D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
      // Indices are stored as absolute offsets in the frame vertex buffer.
      ctx_->DrawIndexed(command.index_count, command.index_offset, 0);
    }
  }

  const HRESULT hr = swap_->Present(0, 0);  // no vsync wait in lab demos
  return FAILED(hr) ? Result<void>("swap-chain present failed") : Result<void>();
}

Result<void> D3D11RenderPipeline::draw_radar_background(float cx, float cy, float radius,
                                   uint32_t bg_color, uint32_t border_color)
{
  push_circle(cx, cy, radius * 1.02f, border_color, 48);
  push_circle(cx, cy, radius, bg_color, 48);
  push_line(cx - radius, cy, cx + radius, cy, border_color);
  push_line(cx, cy - radius, cx, cy + radius, border_color);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_player_dot(float x, float y, float radius, uint32_t color)
{
  push_circle(x, y, radius, color, 20);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_player_arrow(float x, float y, float angle, float size,
                               uint32_t color)
{
  const float s = std::sin(angle);
  const float c = std::cos(angle);
  push_triangle(x + c * size, y + s * size,
                x + s * size * 0.5f, y - c * size * 0.5f,
                x - s * size * 0.5f, y + c * size * 0.5f, color);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_health_bar(float x, float y, float width, float height,
                             int health)
{
  const int clamped_health = std::clamp(health, 0, 100);
  const float percent = static_cast<float>(clamped_health) / 100.0f;
  const uint32_t color = clamped_health > 50 ? 0xFF00FF00
                       : clamped_health > 25 ? 0xFFFF8800 : 0xFFFF0000;
  // Avoid pure black (color-keyed); use dark translucent slate.
  push_quad(x, y, x + width, y + height, 0x88202838);
  push_quad(x, y, x + width * percent, y + height, color);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_health_ring(float x, float y, float radius, int health,
                              int armor)
{
  const int hp = std::clamp(health, 0, 100);
  const int ar = std::clamp(armor, 0, 100);
  const uint32_t hp_col = hp > 50 ? 0xFF33FF55 : hp > 25 ? 0xFFFFAA22 : 0xFFFF3333;
  push_ring(x, y, radius, hp_col, 20);
  if (ar > 0) {
    push_ring(x, y, radius * 1.35f, 0xFF4488FF, 16);
  }
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_text(float x, float y, const char* text, uint32_t color,
                       float size)
{
  if (!text || size <= 0.0f) return Result<void>();
  // Proportional character width map for basic bitmap rendering.
  // Each entry gives the character's width as a fraction of 'size'.
  // Widths for ASCII 32-126, with space = 0.12f and 'M'/'W' = 0.22f
  // ASCII 32..126 inclusive = 95 entries
  static const float kCharWidths[95] = {
      0.12f, 0.08f, 0.12f, 0.18f, 0.16f, 0.20f, 0.18f, 0.06f, //  !"#$%&'
      0.08f, 0.08f, 0.16f, 0.14f, 0.06f, 0.12f, 0.06f, 0.12f, // ()*+,-./
      0.14f, 0.10f, 0.12f, 0.12f, 0.12f, 0.12f, 0.12f, 0.12f, // 01234567
      0.12f, 0.12f, 0.06f, 0.06f, 0.14f, 0.14f, 0.14f, 0.16f, // 89:;<=>?
      0.18f, 0.16f, 0.16f, 0.18f, 0.16f, 0.14f, 0.18f, 0.18f, // @ABCDEFG
      0.08f, 0.12f, 0.16f, 0.14f, 0.22f, 0.18f, 0.18f, 0.16f, // HIJKLMNO
      0.18f, 0.16f, 0.16f, 0.16f, 0.18f, 0.16f, 0.20f, 0.16f, // PQRSTUVW
      0.14f, 0.16f, 0.14f, 0.10f, 0.12f, 0.10f, 0.12f, 0.14f, // XYZ[\]^_
      0.08f, 0.14f, 0.12f, 0.12f, 0.14f, 0.12f, 0.10f, 0.14f, // `abcdefg
      0.14f, 0.06f, 0.08f, 0.12f, 0.06f, 0.20f, 0.14f, 0.14f, // hijklmno
      0.14f, 0.14f, 0.10f, 0.14f, 0.12f, 0.16f, 0.12f, 0.16f, // pqrstuvw
      0.12f, 0.10f, 0.12f, 0.10f, 0.08f, 0.10f, 0.08f        // xyz{|}  (no trailing ~ extra)
  };
  // Render each character as a proportional quad.
  // Each quad is (char_width × size) at cursor position.
  // The quad is a rectangle: upper-left = (cursor, y), lower-right = (cursor + w, y + size)
  // where w = kCharWidths[character - 32] * size
  float cursor_x = x;
  for (const char* cp = text; *cp; ++cp) {
      unsigned char c = static_cast<unsigned char>(*cp);
      if (c < 32 || c > 126) { cursor_x += size * 0.12f; continue; }
      float cw = kCharWidths[c - 32] * size;
      if (c != ' ') {
          // Draw a rectangle for the character at (cursor_x, y) with width cw, height size
          push_quad(cursor_x, y, cursor_x + cw, y + size, color);
      }
      cursor_x += cw;
  }
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_line(float x1, float y1, float x2, float y2, uint32_t color)
{
  push_line(x1, y1, x2, y2, color);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_rect_filled(float x, float y, float w, float h,
                              uint32_t color)
{
  if (w <= 0.0f || h <= 0.0f) return Result<void>();
  // Two triangles forming the quad via the shared push_quad helper.
  push_quad(x, y, x + w, y + h, color);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_rect(float x, float y, float w, float h, uint32_t color,
                       float thickness)
{
  (void)thickness;  // constant line thickness in this pipeline
  if (w <= 0.0f || h <= 0.0f) return Result<void>();
  // Top / bottom / left / right edges as a single LineList batch.
  push_line(x, y, x + w, y, color);
  push_line(x, y + h, x + w, y + h, color);
  push_line(x, y, x, y + h, color);
  push_line(x + w, y, x + w, y + h, color);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_crosshair(float x, float y, float size, uint32_t color)
{
  push_line(x - size * 0.5f, y, x + size * 0.5f, y, color);
  push_line(x, y - size * 0.5f, x, y + size * 0.5f, color);
  return Result<void>();
}

Result<void> D3D11RenderPipeline::draw_radar_frame(const RadarFrame& frame)
{
  constexpr float center_x = 0.0f;
  constexpr float center_y = 0.0f;
  // In-game snap: blips only — never fill a dark disc over the minimap.
  const float radius =
      (style_.anchor == OverlayStyle::Anchor::InGameRadar) ? 0.96f : 0.88f;
  const float usable_radius = radius * 0.92f;

  // Hard-disable filled disc for InGameRadar (prevents dark plate if style drifts).
  const bool want_bg =
      style_.draw_background &&
      style_.anchor != OverlayStyle::Anchor::InGameRadar;
  if (want_bg || style_.draw_border || style_.debug_range_ring) {
    // Never pure black (chroma-keyed / dark plate). Use translucent slate only
    // for non-snap lab modes.
    const std::uint32_t bg =
        want_bg ? (style_.radar_bg == 0 ? 0x55202838u : style_.radar_bg)
                : 0x00000000u;
    std::uint32_t border = style_.draw_border ? style_.radar_border : 0x00000000u;
    if (style_.debug_range_ring && border == 0) {
      border = 0x5533AACC;
    }
    if ((bg & 0xFF000000u) != 0 || (border & 0xFF000000u) != 0) {
      draw_radar_background(center_x, center_y, radius, bg, border);
    }
    if (style_.debug_range_ring) {
      push_ring(center_x, center_y, usable_radius * 0.5f, 0x4433AACC, 28);
    }
  }

  for (const RadarBlipLayout& blip : frame.blips) {
    if (blip.is_local) {
      draw_player_arrow(center_x, center_y, blip.angle, radius * 0.10f,
                        0xFFFFFFFF);
      continue;
    }

    float x = center_x + blip.screen_x * usable_radius;
    float y = center_y + blip.screen_y * usable_radius;
    const float distance = std::sqrt(x * x + y * y);
    if (distance > usable_radius && distance > 0.0f) {
      x = x / distance * usable_radius;
      y = y / distance * usable_radius;
    }
    const std::uint32_t col = blip.color | 0xFF000000u;

    if (blip.is_bomb) {
      // Diamond-ish: two small triangles via arrow + opposite
      draw_player_dot(x, y, 0.04f, 0xFFFFFF22);
      draw_crosshair(x, y, 0.06f, 0xFFFFFF44);
    } else if (blip.is_hostage) {
      draw_player_dot(x, y, 0.028f, 0xFF88FF88);
    } else if (style_.draw_enemy_arrows) {
      draw_player_arrow(x, y, blip.angle, radius * 0.055f, col);
    } else {
      draw_player_dot(x, y, 0.032f, col);
    }

    if (style_.draw_health_rings && !blip.is_bomb && !blip.is_hostage) {
      draw_health_ring(x, y, 0.045f, blip.health, blip.armor);
    } else if (style_.draw_background) {
      draw_health_bar(x - 0.028f, y + 0.035f, 0.055f, 0.009f, blip.health);
    }
    if (blip.label[0] != '\0' && (style_.text & 0xFF000000u) != 0) {
      draw_text(x + 0.03f, y - 0.012f, blip.label, blip.color, 0.025f);
    }
  }

  if (style_.draw_crosshair) {
    draw_crosshair(center_x, center_y, 0.045f, style_.crosshair);
  }
  return Result<void>();
}

bool D3D11RenderPipeline::process_messages()
{
  maintain_overlay();
  MSG message = {};
  while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
    if (message.message == WM_QUIT || message.message == WM_CLOSE ||
        message.message == WM_DESTROY ||
        (message.message == WM_KEYDOWN && message.wParam == VK_END)) return false;
    TranslateMessage(&message);
    DispatchMessageA(&message);
  }
  return true;
}

void D3D11RenderPipeline::add_command(DrawCommand::Type type, uint32_t vertex_offset, uint32_t vertex_count,
                 uint32_t index_offset, uint32_t index_count)
{
  commands_.push_back({type, vertex_offset, vertex_count, index_offset, index_count});
}

void D3D11RenderPipeline::push_quad(float x1, float y1, float x2, float y2, uint32_t color)
{
  const uint32_t vertex_offset = static_cast<uint32_t>(vertices_.size());
  const uint32_t index_offset = static_cast<uint32_t>(indices_.size());
  vertices_.insert(vertices_.end(), {{x1, -y1, 0, 0, color}, {x2, -y1, 0, 0, color},
                                     {x1, -y2, 0, 0, color}, {x2, -y2, 0, 0, color}});
  indices_.insert(indices_.end(), {static_cast<uint16_t>(vertex_offset), static_cast<uint16_t>(vertex_offset + 1), static_cast<uint16_t>(vertex_offset + 2),
                                   static_cast<uint16_t>(vertex_offset + 1), static_cast<uint16_t>(vertex_offset + 3), static_cast<uint16_t>(vertex_offset + 2)});
  add_command(DrawCommand::TriangleList, vertex_offset, 4, index_offset, 6);
}

void D3D11RenderPipeline::push_ring(float cx, float cy, float radius, uint32_t color, int segments)
{
  if (segments < 3 || radius <= 0.0f) return;
  for (int i = 0; i < segments; ++i) {
    const float a0 = static_cast<float>(i) / segments * render_pipeline_detail::kPi * 2.0f;
    const float a1 = static_cast<float>(i + 1) / segments * render_pipeline_detail::kPi * 2.0f;
    push_line(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius,
              cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, color);
  }
}

void D3D11RenderPipeline::push_circle(float cx, float cy, float radius, uint32_t color, int segments)
{
  const uint32_t vertex_offset = static_cast<uint32_t>(vertices_.size());
  const uint32_t index_offset = static_cast<uint32_t>(indices_.size());
  vertices_.push_back({cx, -cy, 0, 0, color});
  for (int index = 0; index <= segments; ++index) {
    const float angle = static_cast<float>(index) / segments * render_pipeline_detail::kPi * 2.0f;
    vertices_.push_back({cx + std::cos(angle) * radius, -(cy + std::sin(angle) * radius), 0, 0, color});
  }
  for (int index = 0; index < segments; ++index) {
    indices_.insert(indices_.end(), {static_cast<uint16_t>(vertex_offset),
        static_cast<uint16_t>(vertex_offset + 1 + index),
        static_cast<uint16_t>(vertex_offset + 2 + index)});
  }
  add_command(DrawCommand::TriangleList, vertex_offset, static_cast<uint32_t>(segments + 2),
              index_offset, static_cast<uint32_t>(segments * 3));
}

void D3D11RenderPipeline::push_line(float x1, float y1, float x2, float y2, uint32_t color)
{
  const uint32_t vertex_offset = static_cast<uint32_t>(vertices_.size());
  const uint32_t index_offset = static_cast<uint32_t>(indices_.size());
  vertices_.insert(vertices_.end(), {{x1, -y1, 0, 0, color}, {x2, -y2, 0, 0, color}});
  indices_.insert(indices_.end(), {static_cast<uint16_t>(vertex_offset), static_cast<uint16_t>(vertex_offset + 1)});
  add_command(DrawCommand::LineList, vertex_offset, 2, index_offset, 2);
}

void D3D11RenderPipeline::push_triangle(float x1, float y1, float x2, float y2, float x3, float y3,
                   uint32_t color)
{
  const uint32_t vertex_offset = static_cast<uint32_t>(vertices_.size());
  const uint32_t index_offset = static_cast<uint32_t>(indices_.size());
  vertices_.insert(vertices_.end(), {{x1, -y1, 0, 0, color}, {x2, -y2, 0, 0, color}, {x3, -y3, 0, 0, color}});
  indices_.insert(indices_.end(), {static_cast<uint16_t>(vertex_offset), static_cast<uint16_t>(vertex_offset + 1), static_cast<uint16_t>(vertex_offset + 2)});
  add_command(DrawCommand::TriangleList, vertex_offset, 3, index_offset, 3);
}

#endif  // LR_PLATFORM_WINDOWS

}  // namespace real::gpu
