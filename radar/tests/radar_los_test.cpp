// radar_los_test.cpp: R11: BVH line-of-sight parity against brute force on
// synthetic scenes (no game assets required).

#include "radar/los_bvh.hpp"

#include <cstdio>
#include <random>

namespace {

using radar_domain::LosBvh;
using radar_domain::Triangle;
using radar_domain::WorldPosition;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

[[maybe_unused]] Triangle triangle(float x, float y, float z, float size) {
  return Triangle{WorldPosition{x, y, z},
                  WorldPosition{x + size, y, z},
                  WorldPosition{x, y + size, z}};
}

void test_empty_scene() {
  LosBvh bvh({});
  WorldPosition a{0, 0, 0};
  WorldPosition b{10, 10, 10};
  check(!bvh.segment_blocked(a, b), "empty scene never blocked");
  check(!bvh.segment_blocked_brute(a, b), "brute agrees on empty");
  check(bvh.node_count() == 0, "no nodes for empty soup");
}

void test_known_walls() {
  // A wall: a grid of coplanar triangles at x = 5, spanning y/z.
  std::vector<Triangle> wall;
  for (float y = -50.0f; y <= 50.0f; y += 5.0f) {
    for (float z = 0.0f; z <= 50.0f; z += 5.0f) {
      wall.push_back(Triangle{WorldPosition{5, y, z},
                              WorldPosition{5, y + 5, z},
                              WorldPosition{5, y, z + 5}});
    }
  }
  const LosBvh bvh(wall);

  // Segment crossing the wall is blocked.
  check(bvh.segment_blocked(WorldPosition{0, 0, 25}, WorldPosition{10, 0, 25}),
        "crossing segment blocked");
  check(bvh.segment_blocked_brute(WorldPosition{0, 0, 25},
                                  WorldPosition{10, 0, 25}),
        "brute agrees: blocked");

  // Segment parallel to the wall on one side is clear.
  check(!bvh.segment_blocked(WorldPosition{0, -40, 25},
                             WorldPosition{0, 40, 25}),
        "parallel segment clear");
  check(!bvh.segment_blocked(WorldPosition{2, 0, 25}, WorldPosition{4, 0, 25}),
        "same-side segment clear");

  // Segment ending exactly at the wall touches it.
  check(bvh.segment_blocked(WorldPosition{0, 0, 25}, WorldPosition{5, 0, 25}),
        "endpoint on wall counts as blocked");
}

void test_brute_parity_random() {
  // 400 pseudo-random triangles; 400 pseudo-random segments; BVH must agree
  // with brute force on every query (both directions).
  std::mt19937 rng(20261105);
  std::uniform_real_distribution<float> span(-100.0f, 100.0f);
  std::uniform_real_distribution<float> size(1.0f, 8.0f);

  std::vector<Triangle> soup;
  soup.reserve(400);
  for (int i = 0; i < 400; ++i) {
    const float x = span(rng);
    const float y = span(rng);
    const float z = span(rng);
    const float s = size(rng);
    soup.push_back(Triangle{WorldPosition{x, y, z},
                            WorldPosition{x + s, y + s * 0.5f, z},
                            WorldPosition{x + s * 0.5f, y, z + s}});
  }
  const LosBvh bvh(soup);

  int blocked_count = 0;
  for (int q = 0; q < 400; ++q) {
    const WorldPosition a{span(rng), span(rng), span(rng)};
    const WorldPosition b{span(rng), span(rng), span(rng)};
    const bool fast = bvh.segment_blocked(a, b);
    const bool slow = bvh.segment_blocked_brute(a, b);
    if (fast != slow) {
      char label[64];
      std::snprintf(label, sizeof(label), "parity query %d", q);
      check(false, label);
      return;
    }
    if (fast) {
      ++blocked_count;
    }
  }
  // Sanity: the corpus must exercise both outcomes, otherwise parity is
  // vacuous.
  check(blocked_count > 0, "some queries blocked");
  check(blocked_count < 400, "some queries clear");
}

void test_single_triangle_boundary() {
  const std::vector<Triangle> one{
      Triangle{WorldPosition{0, 0, 0}, WorldPosition{10, 0, 0},
               WorldPosition{0, 10, 0}}};
  const LosBvh bvh(one);
  check(bvh.segment_blocked(WorldPosition{1, 1, -5}, WorldPosition{1, 1, 5}),
        "through triangle");
  check(!bvh.segment_blocked(WorldPosition{20, 20, -5},
                             WorldPosition{20, 20, 5}),
        "outside triangle");
  check(bvh.segment_blocked_brute(WorldPosition{1, 1, -5},
                                  WorldPosition{1, 1, 5}),
        "brute agrees through");
}

}  // namespace

int main() {
  test_empty_scene();
  test_known_walls();
  test_brute_parity_random();
  test_single_triangle_boundary();

  if (failures != 0) {
    std::printf("%d los check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_los_test: all checks passed\n");
  return 0;
}
