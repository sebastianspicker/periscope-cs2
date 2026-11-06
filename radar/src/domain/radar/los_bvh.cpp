// los_bvh.cpp — R11 map-collision BVH and segment queries. See los_bvh.hpp.

#include "radar/los_bvh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace radar_domain {
namespace {

void expand_bounds(BvhNode& node, const WorldPosition& p) {
  node.min_x = std::min(node.min_x, p.x);
  node.min_y = std::min(node.min_y, p.y);
  node.min_z = std::min(node.min_z, p.z);
  node.max_x = std::max(node.max_x, p.x);
  node.max_y = std::max(node.max_y, p.y);
  node.max_z = std::max(node.max_z, p.z);
}

bool segment_aabb_overlap(const WorldPosition& a, const WorldPosition& b,
                          const BvhNode& node) {
  // Slab method for the segment [a, b] against the node box.
  float t_min = 0.0f;
  float t_max = 1.0f;
  const float dir[3] = {b.x - a.x, b.y - a.y, b.z - a.z};
  const float origin[3] = {a.x, a.y, a.z};
  const float box_min[3] = {node.min_x, node.min_y, node.min_z};
  const float box_max[3] = {node.max_x, node.max_y, node.max_z};
  for (int axis = 0; axis < 3; ++axis) {
    if (std::abs(dir[axis]) < 1e-12f) {
      if (origin[axis] < box_min[axis] || origin[axis] > box_max[axis]) {
        return false;
      }
      continue;
    }
    float t1 = (box_min[axis] - origin[axis]) / dir[axis];
    float t2 = (box_max[axis] - origin[axis]) / dir[axis];
    if (t1 > t2) {
      std::swap(t1, t2);
    }
    t_min = std::max(t_min, t1);
    t_max = std::min(t_max, t2);
    if (t_min > t_max) {
      return false;
    }
  }
  return true;
}

// Möller–Trumbore segment/triangle intersection.
bool segment_triangle_hit(const WorldPosition& a, const WorldPosition& b,
                          const Triangle& tri) {
  const float e1x = tri[1].x - tri[0].x;
  const float e1y = tri[1].y - tri[0].y;
  const float e1z = tri[1].z - tri[0].z;
  const float e2x = tri[2].x - tri[0].x;
  const float e2y = tri[2].y - tri[0].y;
  const float e2z = tri[2].z - tri[0].z;

  const float px = (b.y - a.y) * e2z - (b.z - a.z) * e2y;
  const float py = (b.z - a.z) * e2x - (b.x - a.x) * e2z;
  const float pz = (b.x - a.x) * e2y - (b.y - a.y) * e2x;

  const float det = e1x * px + e1y * py + e1z * pz;
  if (std::abs(det) < 1e-12f) {
    return false;  // parallel or degenerate
  }
  const float inv_det = 1.0f / det;

  const float tx = a.x - tri[0].x;
  const float ty = a.y - tri[0].y;
  const float tz = a.z - tri[0].z;

  const float u = (tx * px + ty * py + tz * pz) * inv_det;
  if (u < -1e-6f || u > 1.0f + 1e-6f) {
    return false;
  }

  const float qx = ty * e1z - tz * e1y;
  const float qy = tz * e1x - tx * e1z;
  const float qz = tx * e1y - ty * e1x;

  const float v =
      ((b.x - a.x) * qx + (b.y - a.y) * qy + (b.z - a.z) * qz) * inv_det;
  if (v < -1e-6f || u + v > 1.0f + 1e-6f) {
    return false;
  }

  // Distance along the segment, normalized to [0, 1].
  const float t = (e2x * qx + e2y * qy + e2z * qz) * inv_det;
  return t >= -1e-6f && t <= 1.0f + 1e-6f;
}

}  // namespace

LosBvh::LosBvh(const std::vector<Triangle>& triangles)
    : triangles_(triangles) {
  ordered_indices_.resize(triangles_.size());
  for (std::size_t i = 0; i < ordered_indices_.size(); ++i) {
    ordered_indices_[i] = static_cast<std::uint32_t>(i);
  }
  if (!triangles_.empty()) {
    nodes_.reserve(triangles_.size() * 2 + 1);
    build(0, ordered_indices_.size());
  }
}

std::uint32_t LosBvh::build(std::size_t begin, std::size_t end) {
  const std::uint32_t node_index = static_cast<std::uint32_t>(nodes_.size());
  nodes_.emplace_back();
  BvhNode& node = nodes_[node_index];
  // Start from inverted bounds so the box fits the triangles, not the origin.
  node.min_x = node.min_y = node.min_z = std::numeric_limits<float>::max();
  node.max_x = node.max_y = node.max_z = std::numeric_limits<float>::lowest();

  for (std::size_t i = begin; i < end; ++i) {
    const Triangle& tri = triangles_[ordered_indices_[i]];
    for (const WorldPosition& p : tri) {
      expand_bounds(node, p);
    }
  }

  const std::size_t count = end - begin;
  if (count <= kLeafSize) {
    node.leaf = true;
    // The leaf's triangles are exactly ordered_indices_[begin, end) — the
    // recursive nth_element partition guarantees each leaf owns a contiguous
    // window of the persistent ordered index buffer.
    node.first_triangle = static_cast<std::uint32_t>(begin);
    node.triangle_count = static_cast<std::uint32_t>(count);
    return node_index;
  }

  // Median split on the largest centroid-axis extent.
  float min_c[3] = {std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()};
  float max_c[3] = {std::numeric_limits<float>::lowest(),
                    std::numeric_limits<float>::lowest(),
                    std::numeric_limits<float>::lowest()};
  for (std::size_t i = begin; i < end; ++i) {
    const Triangle& tri = triangles_[ordered_indices_[i]];
    const float cx = (tri[0].x + tri[1].x + tri[2].x) / 3.0f;
    const float cy = (tri[0].y + tri[1].y + tri[2].y) / 3.0f;
    const float cz = (tri[0].z + tri[1].z + tri[2].z) / 3.0f;
    min_c[0] = std::min(min_c[0], cx);
    min_c[1] = std::min(min_c[1], cy);
    min_c[2] = std::min(min_c[2], cz);
    max_c[0] = std::max(max_c[0], cx);
    max_c[1] = std::max(max_c[1], cy);
    max_c[2] = std::max(max_c[2], cz);
  }
  const float extent[3] = {max_c[0] - min_c[0], max_c[1] - min_c[1],
                           max_c[2] - min_c[2]};
  const int axis =
      extent[0] >= extent[1] && extent[0] >= extent[2]
          ? 0
          : (extent[1] >= extent[2] ? 1 : 2);

  const std::size_t mid = begin + count / 2;
  auto centroid_axis = [this, axis](std::uint32_t index) {
    const Triangle& tri = triangles_[index];
    const float c[3] = {(tri[0].x + tri[1].x + tri[2].x) / 3.0f,
                        (tri[0].y + tri[1].y + tri[2].y) / 3.0f,
                        (tri[0].z + tri[1].z + tri[2].z) / 3.0f};
    return c[axis];
  };
  std::nth_element(ordered_indices_.begin() +
                       static_cast<std::ptrdiff_t>(begin),
                   ordered_indices_.begin() + static_cast<std::ptrdiff_t>(mid),
                   ordered_indices_.begin() + static_cast<std::ptrdiff_t>(end),
                   [&](std::uint32_t lhs, std::uint32_t rhs) {
                     return centroid_axis(lhs) < centroid_axis(rhs);
                   });

  const std::uint32_t left = build(begin, mid);
  const std::uint32_t right = build(mid, end);
  nodes_[node_index].left = left;
  nodes_[node_index].right = right;
  return node_index;
}

bool LosBvh::visit(std::uint32_t node_index, const WorldPosition& a,
                   const WorldPosition& b) const {
  const BvhNode& node = nodes_[node_index];
  if (!segment_aabb_overlap(a, b, node)) {
    return false;
  }
  if (node.leaf) {
    for (std::uint32_t i = 0; i < node.triangle_count; ++i) {
      const std::uint32_t tri_index =
          ordered_indices_[node.first_triangle + i];
      if (segment_triangle_hit(a, b, triangles_[tri_index])) {
        return true;
      }
    }
    return false;
  }
  return visit(node.left, a, b) || visit(node.right, a, b);
}

bool LosBvh::segment_blocked(const WorldPosition& a,
                             const WorldPosition& b) const {
  if (nodes_.empty()) {
    return false;
  }
  return visit(0, a, b);
}

bool LosBvh::segment_blocked_brute(const WorldPosition& a,
                                   const WorldPosition& b) const {
  for (const Triangle& tri : triangles_) {
    if (segment_triangle_hit(a, b, tri)) {
      return true;
    }
  }
  return false;
}

}  // namespace radar_domain
