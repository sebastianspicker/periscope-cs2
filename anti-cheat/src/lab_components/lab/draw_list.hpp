#pragma once

// Educational simulation: minimal draw list API.
// Avoids ImGui signature in binary (demonstrates real-world practice).

#include "ac/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace lab {

enum class PrimitiveType : std::uint8_t {
  Line,
  Rect,
  RectFilled,
  CircleFilled,
  Triangle,
  Polyline,
  Text,
  Arrow,
};

struct DrawCommand {
  PrimitiveType type = PrimitiveType::Line;
  float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
  float x3 = 0, y3 = 0;  // triangle third vertex / arrow head
  std::uint32_t color = 0xFFFFFFFF;
  float thickness = 1.0f;
  float radius = 0.0f;
  std::string text;
  std::vector<ac::Vec2> points;  // polyline
};

struct DrawListStats {
  int total_commands = 0;
  int lines = 0;
  int rects = 0;
  int filled_rects = 0;
  int circles = 0;
  int triangles = 0;
  int polylines = 0;
  int texts = 0;
  int arrows = 0;
  int unique_colors = 0;
  bool looks_like_esp = false;  // multi-primitive ESP signature
  std::string detail;
};

// 4x4 row-major view-projection matrix for world→screen.
struct ViewProjection {
  float m[16]{};
  static ViewProjection identity();
  static ViewProjection orthographic(float left, float right, float bottom,
                                     float top, float znear, float zfar);
};

struct ScreenPoint {
  float x = 0, y = 0;
  bool on_screen = false;
  float depth = 0;
};

class DrawList {
 public:
  void line(float x1, float y1, float x2, float y2, std::uint32_t color,
            float t = 1.0f);
  void rect(float x, float y, float w, float h, std::uint32_t color,
            float t = 1.0f);
  void rect_filled(float x, float y, float w, float h, std::uint32_t color);
  void circle(float cx, float cy, float r, std::uint32_t color);
  void triangle(float x1, float y1, float x2, float y2, float x3, float y3,
                std::uint32_t color, float t = 1.0f);
  void polyline(const std::vector<ac::Vec2>& pts, std::uint32_t color,
                float t = 1.0f);
  void text(float x, float y, const std::string& t, std::uint32_t color);
  void arrow(float x1, float y1, float x2, float y2, std::uint32_t color,
             float t = 1.0f);

  // ESP helpers used by overlay radar lessons.
  void box_2d(float x, float y, float w, float h, std::uint32_t color,
              float t = 1.0f);
  void entity_marker(float sx, float sy, std::uint8_t team, bool alive,
                     const std::string& label = {});

  // Project world position through VP matrix into screen space.
  static ScreenPoint world_to_screen(const ac::Vec3& world,
                                     const ViewProjection& vp, float screen_w,
                                     float screen_h);

  const std::vector<DrawCommand>& commands() const { return commands_; }
  DrawListStats stats() const;
  void clear();

 private:
  std::vector<DrawCommand> commands_;
};

}  // namespace lab
