// view_point.hpp — Eye-origin derivation for LOS and direction (E48).
//
// A segment from the feet is blocked by the player's own cover. The eye is
// the correct origin for "can I see them". `m_vecViewOffset` is a small
// local-space offset; the camera block is the fallback. Both are validated
// before use so a torn read cannot put the eye in the next map.
//
// Pure and host-testable.

#pragma once

#include "ac/types.hpp"

#include <cmath>

namespace real::cs2::view {

struct EyePoint {
  ac::Vec3 origin{};
  bool valid = false;
};

/// True when a view offset is a plausible standing/crouching eye offset.
inline bool plausible_offset(const ac::Vec3& offset) {
  if (!std::isfinite(offset.x) || !std::isfinite(offset.y) ||
      !std::isfinite(offset.z)) {
    return false;
  }
  // Source eye offsets are roughly (0, 0, 55..75). Be loose: +/-32 horizontal,
  // 32..96 vertical. A zero offset is treated as unavailable.
  const float horizontal = std::sqrt(offset.x * offset.x + offset.y * offset.y);
  if (horizontal > 32.0f) return false;
  if (offset.z < 32.0f || offset.z > 96.0f) return false;
  return true;
}

/// Derive the eye point from an origin and an optional view offset. Falls
/// back to a fixed standing eye height when the offset is unusable.
inline EyePoint eye_from(const ac::Vec3& origin, const ac::Vec3& offset,
                         bool offset_known, float fallback_height = 64.0f) {
  EyePoint p;
  p.origin = origin;
  if (offset_known && plausible_offset(offset)) {
    p.origin.x += offset.x;
    p.origin.y += offset.y;
    p.origin.z += offset.z;
    p.valid = true;
    return p;
  }
  if (std::isfinite(origin.x) && std::isfinite(origin.y) &&
      std::isfinite(origin.z)) {
    p.origin.z += fallback_height;
    p.valid = true;
  }
  return p;
}

/// True when a camera FOV is usable for projection/LOS.
inline bool plausible_fov(float fov) {
  return std::isfinite(fov) && fov > 0.0f && fov <= 170.0f;
}

}  // namespace real::cs2::view
