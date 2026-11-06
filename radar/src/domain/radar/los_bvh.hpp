// los_bvh.hpp — R11: map-collision BVH and line-of-sight queries.
//
// Adapted from the research ledger (R11): oasis map collision loading
// (`.tri` triangle soup + BVH acceleration), deadlocked `cs2/bvh.rs`, and
// chao-shushu's `VisibilityChecker`. Pure geometry over a triangle soup —
// the extraction pipeline that produces the soup from game assets is a
// script concern and lives in radar/scripts (ledger R11 lands that
// separately); this module only consumes triangles.
//
// Domain layer: pure std, no process/hardware access.

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "radar/snapshot.hpp"

namespace radar_domain {

using Triangle = std::array<WorldPosition, 3>;

struct BvhNode {
  // Node bounds in world coordinates.
  float min_x = 0.0f;
  float min_y = 0.0f;
  float min_z = 0.0f;
  float max_x = 0.0f;
  float max_y = 0.0f;
  float max_z = 0.0f;
  std::uint32_t left = 0;        // child index; internal nodes only
  std::uint32_t right = 0;
  std::uint32_t first_triangle = 0;  // leaf range into the triangle index
  std::uint32_t triangle_count = 0;
  bool leaf = false;
};

// Bounding-volume hierarchy over a triangle soup with median splits on the
// largest centroid axis. Query cost is O(log n + hits); parity with brute
// force is a test requirement (see tests/radar_los_test.cpp).
class LosBvh {
 public:
  static constexpr std::size_t kLeafSize = 4;

  // Builds the BVH. An empty soup produces an empty tree; every query
  // reports unobstructed.
  explicit LosBvh(const std::vector<Triangle>& triangles);

  // True when the open segment from `a` to `b` intersects any triangle.
  [[nodiscard]] bool segment_blocked(const WorldPosition& a,
                                     const WorldPosition& b) const;

  // Brute-force reference for the same query (used by tests and as a
  // small-scene fallback).
  [[nodiscard]] bool segment_blocked_brute(const WorldPosition& a,
                                           const WorldPosition& b) const;

  [[nodiscard]] std::size_t node_count() const { return nodes_.size(); }
  [[nodiscard]] std::size_t triangle_count() const { return triangles_.size(); }

 private:
  std::vector<Triangle> triangles_;
  std::vector<std::uint32_t> ordered_indices_;
  std::vector<BvhNode> nodes_;

  std::uint32_t build(std::size_t begin, std::size_t end);
  [[nodiscard]] bool visit(std::uint32_t node_index, const WorldPosition& a,
                           const WorldPosition& b) const;
};

}  // namespace radar_domain
