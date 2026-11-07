// world_collision.hpp: live vphys2 mesh-BVH extraction (E47).
//
// Tier-2 LOS source. The game already holds the current map's collision world;
// this walks it through the external read functor instead of shipping a
// pre-baked triangle file. It tracks map changes and includes brushes a baked
// file can miss.
//
// Scope: mesh shapes only. Hull shapes (CRnHullShape) need a separate hedge/
// face walk and are a documented follow-up. Mesh shapes are what walls and
// floors are made of, so LOS works; small hull-only props are missed.
//
// Layouts are the donor's research offsets and are version-fragile. Every
// count is bounded and every read is checked; a corrupt body is skipped, not
// trusted.
//
// Pure over a read functor and host-testable with a synthetic image.

#pragma once

#include "radar/los_bvh.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace real::cs2::collision {

inline constexpr std::uint64_t kMaxPhysical = 0x7FFF'FFFF'FFFFull;
inline constexpr std::size_t kMaxTriangles = 1u << 21;  // 2M

struct MeshTransform {
  float rot[3][3]{};
  float scale[3]{1.0f, 1.0f, 1.0f};
  float pos[3]{};
};

struct ExtractResult {
  bool ok = false;
  std::size_t bodies = 0;
  std::size_t triangles = 0;
  std::string error;
};

inline bool finite3(const float v[3]) {
  return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

inline void quat_to_matrix(const float q[4], float out[3][3]) {
  const float xx = q[0] * q[0], yy = q[1] * q[1], zz = q[2] * q[2];
  const float xy = q[0] * q[1], xz = q[0] * q[2], yz = q[1] * q[2];
  const float wx = q[3] * q[0], wy = q[3] * q[1], wz = q[3] * q[2];
  out[0][0] = 1 - 2 * (yy + zz);
  out[0][1] = 2 * (xy + wz);
  out[0][2] = 2 * (xz - wy);
  out[1][0] = 2 * (xy - wz);
  out[1][1] = 1 - 2 * (xx + zz);
  out[1][2] = 2 * (yz + wx);
  out[2][0] = 2 * (xz + wy);
  out[2][1] = 2 * (yz - wx);
  out[2][2] = 1 - 2 * (xx + yy);
}

inline radar_domain::WorldPosition transform_point(
    const MeshTransform& xf, const float local[3]) {
  const float scaled[3] = {local[0] * xf.scale[0], local[1] * xf.scale[1],
                           local[2] * xf.scale[2]};
  return radar_domain::WorldPosition{
      xf.rot[0][0] * scaled[0] + xf.rot[1][0] * scaled[1] +
          xf.rot[2][0] * scaled[2] + xf.pos[0],
      xf.rot[0][1] * scaled[0] + xf.rot[1][1] * scaled[1] +
          xf.rot[2][1] * scaled[2] + xf.pos[1],
      xf.rot[0][2] * scaled[0] + xf.rot[1][2] * scaled[1] +
          xf.rot[2][2] * scaled[2] + xf.pos[2]};
}

struct Range {
  std::uint32_t start = 0;
  std::uint32_t count = 0;
};

template <typename ReadFn>
bool read_exact(ReadFn& read, std::uint64_t address, void* out,
                std::size_t size) {
  return address != 0 && address < kMaxPhysical &&
         read(address, out, size);
}

template <typename ReadFn>
std::size_t extract_mesh(ReadFn& read, std::uint64_t bvh_ptr,
                         std::uint64_t vert_ptr, std::uint64_t tri_ptr,
                         std::uint32_t node_count, const MeshTransform& xf,
                         std::vector<radar_domain::Triangle>& out,
                         std::size_t budget) {
  if (bvh_ptr == 0 || vert_ptr == 0 || tri_ptr == 0 || node_count == 0 ||
      node_count > 0x1000000 || out.size() >= budget) {
    return 0;
  }
  constexpr std::size_t kNodeSize = 32;
  std::vector<std::uint8_t> nodes(static_cast<std::size_t>(node_count) * kNodeSize);
  if (!read_exact(read, bvh_ptr, nodes.data(), nodes.size())) return 0;

  std::vector<Range> ranges;
  std::vector<std::uint32_t> stack;
  std::uint32_t cursor = 0;
  std::uint32_t min_tri = 0xFFFFFFFFu;
  std::uint32_t max_tri = 0;

  for (;;) {
    if (cursor >= node_count) {
      if (stack.empty()) break;
      cursor = stack.back();
      stack.pop_back();
      continue;
    }
    const std::uint8_t* node = nodes.data() + cursor * kNodeSize;
    std::uint32_t packed0 = 0, packed1 = 0;
    std::memcpy(&packed0, node + 12, 4);
    std::memcpy(&packed1, node + 28, 4);
    const std::uint32_t type = packed0 >> 30;
    const std::uint32_t payload = packed0 & 0x3FFFFFFFu;
    if (type == 3) {
      if (payload > 0 && payload < 0x1000000) {
        ranges.push_back({packed1, payload});
        if (packed1 < min_tri) min_tri = packed1;
        if (packed1 + payload > max_tri) max_tri = packed1 + payload;
      }
      if (stack.empty()) break;
      cursor = stack.back();
      stack.pop_back();
      continue;
    }
    if (payload == 0) {
      if (stack.empty()) break;
      cursor = stack.back();
      stack.pop_back();
      continue;
    }
    if (cursor + payload < node_count) stack.push_back(cursor + payload);
    ++cursor;
  }

  if (ranges.empty() || max_tri <= min_tri) return 0;
  const std::uint32_t total = max_tri - min_tri;
  if (total > 0x1000000) return 0;

  std::vector<std::int32_t> indices(static_cast<std::size_t>(total) * 3);
  if (!read_exact(read, tri_ptr + static_cast<std::uint64_t>(min_tri) * 12,
                  indices.data(), indices.size() * sizeof(std::int32_t))) {
    return 0;
  }
  std::int32_t max_vert = 0;
  for (const std::int32_t index: indices) {
    if (index > max_vert) max_vert = index;
  }
  if (max_vert <= 0 || max_vert > 0x1000000) return 0;

  const std::uint32_t vert_count = static_cast<std::uint32_t>(max_vert + 1);
  std::vector<float> vertices(static_cast<std::size_t>(vert_count) * 3);
  if (!read_exact(read, vert_ptr, vertices.data(), vertices.size() * sizeof(float))) {
    return 0;
  }

  const std::size_t before = out.size();
  for (const Range& range : ranges) {
    for (std::uint32_t i = 0; i < range.count; ++i) {
      if (out.size() >= budget) break;
      const std::uint32_t local = range.start - min_tri + i;
      if (local >= total) continue;
      const std::int32_t i0 = indices[static_cast<std::size_t>(local) * 3];
      const std::int32_t i1 = indices[static_cast<std::size_t>(local) * 3 + 1];
      const std::int32_t i2 = indices[static_cast<std::size_t>(local) * 3 + 2];
      if (i0 < 0 || i1 < 0 || i2 < 0) continue;
      if (static_cast<std::uint32_t>(i0) >= vert_count ||
          static_cast<std::uint32_t>(i1) >= vert_count ||
          static_cast<std::uint32_t>(i2) >= vert_count) {
        continue;
      }
      out.push_back(radar_domain::Triangle{
          transform_point(xf, &vertices[static_cast<std::size_t>(i0) * 3]),
          transform_point(xf, &vertices[static_cast<std::size_t>(i1) * 3]),
          transform_point(xf, &vertices[static_cast<std::size_t>(i2) * 3])});
    }
  }
  return out.size() - before;
}

template <typename ReadFn>
bool process_mesh_body(ReadFn& read, std::uint64_t body,
                       std::vector<radar_domain::Triangle>& out,
                       std::size_t budget) {
  std::uint64_t mesh_data = 0;
  if (!read_exact(read, body + 0xC0, &mesh_data, sizeof(mesh_data)) ||
      mesh_data == 0) {
    return false;
  }
  std::uint8_t md[0xA0]{};
  if (!read_exact(read, mesh_data, md, sizeof(md))) return false;

  MeshTransform xf{};
  if (!read_exact(read, body + 0xB0, xf.scale, sizeof(xf.scale)) ||
      !finite3(xf.scale) ||
      (xf.scale[0] == 0.0f && xf.scale[1] == 0.0f && xf.scale[2] == 0.0f)) {
    return false;
  }
  if (!read_exact(read, body + 0x100, xf.pos, sizeof(xf.pos)) ||
      !finite3(xf.pos)) {
    return false;
  }
  float quat[4]{0, 0, 0, 1};
  if (!read_exact(read, body + 0x130, quat, sizeof(quat))) return false;
  const float ql = quat[0] * quat[0] + quat[1] * quat[1] + quat[2] * quat[2] +
                   quat[3] * quat[3];
  if (ql < 0.5f || ql > 1.5f) {
    quat[0] = quat[1] = quat[2] = 0.0f;
    quat[3] = 1.0f;
  }
  quat_to_matrix(quat, xf.rot);

  std::uint64_t bvh_ptr = 0, vert_ptr = 0, tri_ptr = 0;
  std::memcpy(&bvh_ptr, md + 0x20, sizeof(bvh_ptr));
  std::memcpy(&vert_ptr, md + 0x38, sizeof(vert_ptr));
  std::memcpy(&tri_ptr, md + 0x50, sizeof(tri_ptr));

  std::uint32_t node_count = 0;
  for (const std::size_t offset: {0x28u, 0x30u, 0x48u, 0x58u}) {
    std::int32_t candidate = 0;
    std::memcpy(&candidate, md + offset, sizeof(candidate));
    if (candidate > 0 && candidate < 0x1000000) {
      node_count = static_cast<std::uint32_t>(candidate);
      break;
    }
  }
  if (node_count == 0) return false;
  return extract_mesh(read, bvh_ptr, vert_ptr, tri_ptr, node_count, xf, out,
                      budget) > 0;
}

/// Walk the world's body array and append every mesh triangle. Hull shapes
/// are skipped by design (see the header comment).
template <typename ReadFn>
ExtractResult extract_world(ReadFn read, std::uint64_t world,
                            std::vector<radar_domain::Triangle>& out,
                            std::size_t max_triangles = kMaxTriangles) {
  ExtractResult result;
  if (world == 0) {
    result.error = "missing world";
    return result;
  }
  std::uint64_t inner = 0;
  if (!read_exact(read, world + 0x30, &inner, sizeof(inner)) || inner == 0) {
    result.error = "missing inner world";
    return result;
  }
  std::uint64_t bodies = 0;
  if (!read_exact(read, inner + 0x110, &bodies, sizeof(bodies)) || bodies == 0) {
    result.error = "missing body array";
    return result;
  }
  std::int32_t count = 0;
  if (!read_exact(read, bodies + 0x268, &count, sizeof(count)) || count <= 0 ||
      count > 0x100000) {
    result.error = "bad body count";
    return result;
  }

  for (std::int32_t i = 0; i < count && out.size() < max_triangles; ++i) {
    const std::uint64_t body = bodies + static_cast<std::uint64_t>(i) * 88;
    std::uint32_t kind = 0;
    if (!read_exact(read, body + 0x40, &kind, sizeof(kind)) || kind != 2) {
      continue;  // hull/other shape
    }
    if (process_mesh_body(read, body, out, max_triangles)) ++result.bodies;
  }
  result.ok = !out.empty();
  if (!result.ok) result.error = "no mesh triangles";
  result.triangles = out.size();
  return result;
}

}  // namespace real::cs2::collision
