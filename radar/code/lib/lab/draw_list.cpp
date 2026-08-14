#include "lab/draw_list.hpp"

#include <cmath>
#include <set>
#include <sstream>

namespace lab {

ViewProjection ViewProjection::identity() {
  ViewProjection vp;
  vp.m[0] = vp.m[5] = vp.m[10] = vp.m[15] = 1.0f;
  return vp;
}

ViewProjection ViewProjection::orthographic(float left, float right,
                                            float bottom, float top,
                                            float znear, float zfar) {
  // Educational positive-Z ortho (lab world coords). Maps
  // x∈[left,right], y∈[bottom,top], z∈[near,far] → NDC cube [-1,1]^3.
  // Column-major layout; multiply as M * [x y z 1]^T.
  ViewProjection vp{};
  const float rl = right - left;
  const float tb = top - bottom;
  const float fn = zfar - znear;
  if (rl == 0.0f || tb == 0.0f || fn == 0.0f) return identity();
  vp.m[0] = 2.0f / rl;
  vp.m[5] = 2.0f / tb;
  vp.m[10] = 2.0f / fn;
  vp.m[12] = -(right + left) / rl;
  vp.m[13] = -(top + bottom) / tb;
  vp.m[14] = -(zfar + znear) / fn;
  vp.m[15] = 1.0f;
  return vp;
}

void DrawList::line(float x1, float y1, float x2, float y2, std::uint32_t color,
                    float t) {
  DrawCommand cmd;
  cmd.type = PrimitiveType::Line;
  cmd.x1 = x1;
  cmd.y1 = y1;
  cmd.x2 = x2;
  cmd.y2 = y2;
  cmd.color = color;
  cmd.thickness = t;
  commands_.push_back(std::move(cmd));
}

void DrawList::rect(float x, float y, float w, float h, std::uint32_t color,
                    float t) {
  DrawCommand cmd;
  cmd.type = PrimitiveType::Rect;
  cmd.x1 = x;
  cmd.y1 = y;
  cmd.x2 = w;
  cmd.y2 = h;
  cmd.color = color;
  cmd.thickness = t;
  commands_.push_back(std::move(cmd));
}

void DrawList::rect_filled(float x, float y, float w, float h,
                           std::uint32_t color) {
  DrawCommand cmd;
  cmd.type = PrimitiveType::RectFilled;
  cmd.x1 = x;
  cmd.y1 = y;
  cmd.x2 = w;
  cmd.y2 = h;
  cmd.color = color;
  commands_.push_back(std::move(cmd));
}

void DrawList::circle(float cx, float cy, float r, std::uint32_t color) {
  DrawCommand cmd;
  cmd.type = PrimitiveType::CircleFilled;
  cmd.x1 = cx;
  cmd.y1 = cy;
  cmd.radius = r;
  cmd.color = color;
  commands_.push_back(std::move(cmd));
}

void DrawList::triangle(float x1, float y1, float x2, float y2, float x3,
                        float y3, std::uint32_t color, float t) {
  DrawCommand cmd;
  cmd.type = PrimitiveType::Triangle;
  cmd.x1 = x1;
  cmd.y1 = y1;
  cmd.x2 = x2;
  cmd.y2 = y2;
  cmd.x3 = x3;
  cmd.y3 = y3;
  cmd.color = color;
  cmd.thickness = t;
  commands_.push_back(std::move(cmd));
}

void DrawList::polyline(const std::vector<ac::Vec2>& pts, std::uint32_t color,
                        float t) {
  DrawCommand cmd;
  cmd.type = PrimitiveType::Polyline;
  cmd.points = pts;
  cmd.color = color;
  cmd.thickness = t;
  commands_.push_back(std::move(cmd));
}

void DrawList::text(float x, float y, const std::string& t,
                    std::uint32_t color) {
  DrawCommand cmd;
  cmd.type = PrimitiveType::Text;
  cmd.x1 = x;
  cmd.y1 = y;
  cmd.color = color;
  cmd.text = t;
  commands_.push_back(std::move(cmd));
}

void DrawList::arrow(float x1, float y1, float x2, float y2,
                     std::uint32_t color, float t) {
  DrawCommand cmd;
  cmd.type = PrimitiveType::Arrow;
  cmd.x1 = x1;
  cmd.y1 = y1;
  cmd.x2 = x2;
  cmd.y2 = y2;
  cmd.color = color;
  cmd.thickness = t;
  // Head offset derived from direction for consumers that render the head.
  const float dx = x2 - x1;
  const float dy = y2 - y1;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len > 1e-3f) {
    cmd.x3 = dx / len;
    cmd.y3 = dy / len;
  }
  commands_.push_back(std::move(cmd));
}

void DrawList::box_2d(float x, float y, float w, float h, std::uint32_t color,
                      float t) {
  rect(x, y, w, h, color, t);
  // Health-bar style filled strip on the left edge.
  rect_filled(x - 4.0f, y, 3.0f, h, (color & 0x00FFFFFF) | 0xA0000000);
}

void DrawList::entity_marker(float sx, float sy, std::uint8_t team, bool alive,
                             const std::string& label) {
  const std::uint32_t color = !alive ? 0xFF808080u
                              : team == 2  ? 0xFF4040FFu
                              : team == 3  ? 0xFFFFA040u
                                           : 0xFF40FF40u;
  circle(sx, sy, alive ? 5.0f : 3.0f, color);
  if (!label.empty()) text(sx + 6.0f, sy - 6.0f, label, color);
}

ScreenPoint DrawList::world_to_screen(const ac::Vec3& world,
                                      const ViewProjection& vp, float screen_w,
                                      float screen_h) {
  ScreenPoint out;
  // Clip-space: row-major multiply [x y z 1] * M
  const float x = world.x * vp.m[0] + world.y * vp.m[4] + world.z * vp.m[8] +
                  vp.m[12];
  const float y = world.x * vp.m[1] + world.y * vp.m[5] + world.z * vp.m[9] +
                  vp.m[13];
  const float z = world.x * vp.m[2] + world.y * vp.m[6] + world.z * vp.m[10] +
                  vp.m[14];
  const float w = world.x * vp.m[3] + world.y * vp.m[7] + world.z * vp.m[11] +
                  vp.m[15];
  if (std::fabs(w) < 1e-6f) return out;
  const float inv_w = 1.0f / w;
  const float ndc_x = x * inv_w;
  const float ndc_y = y * inv_w;
  out.depth = z * inv_w;
  out.x = (ndc_x * 0.5f + 0.5f) * screen_w;
  out.y = (1.0f - (ndc_y * 0.5f + 0.5f)) * screen_h;
  // Accept a small epsilon beyond the clip planes — orthographic near/far
  // edges and float error commonly land just outside exact [-1, 1].
  constexpr float kDepthEps = 1e-3f;
  out.on_screen = out.depth >= -1.0f - kDepthEps &&
                  out.depth <= 1.0f + kDepthEps && out.x >= 0.0f &&
                  out.x <= screen_w && out.y >= 0.0f && out.y <= screen_h;
  return out;
}

DrawListStats DrawList::stats() const {
  DrawListStats s;
  s.total_commands = static_cast<int>(commands_.size());
  std::set<std::uint32_t> colors;
  for (const auto& c : commands_) {
    colors.insert(c.color);
    switch (c.type) {
      case PrimitiveType::Line: ++s.lines; break;
      case PrimitiveType::Rect: ++s.rects; break;
      case PrimitiveType::RectFilled: ++s.filled_rects; break;
      case PrimitiveType::CircleFilled: ++s.circles; break;
      case PrimitiveType::Triangle: ++s.triangles; break;
      case PrimitiveType::Polyline: ++s.polylines; break;
      case PrimitiveType::Text: ++s.texts; break;
      case PrimitiveType::Arrow: ++s.arrows; break;
    }
  }
  s.unique_colors = static_cast<int>(colors.size());
  // ESP signature: boxes/circles + text labels + ≥2 colors (team differentiation).
  s.looks_like_esp =
      (s.rects + s.filled_rects + s.circles) >= 2 && s.texts >= 1 &&
      s.unique_colors >= 2;
  std::ostringstream oss;
  oss << "cmds=" << s.total_commands << " circles=" << s.circles
      << " rects=" << (s.rects + s.filled_rects) << " texts=" << s.texts
      << " colors=" << s.unique_colors
      << " esp=" << (s.looks_like_esp ? 1 : 0);
  s.detail = oss.str();
  return s;
}

void DrawList::clear() { commands_.clear(); }

}  // namespace lab
