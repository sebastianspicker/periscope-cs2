// radar_tri_loader_test.cpp: R11: `.tri` collision file round-trip and
// rejection paths (stream-injected, no filesystem, no game assets).

#include "radar/tri_loader.hpp"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace std::string_literals;

#include "radar/los_bvh.hpp"
#include "radar/tri_loader.hpp"

namespace {

using radar_application::TriLoadResult;
using radar_application::load_tri_stream;
using radar_application::write_tri_stream;
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

void test_round_trip_and_query() {
  const std::vector<Triangle> soup{
      Triangle{WorldPosition{0, 0, 0}, WorldPosition{10, 0, 0},
               WorldPosition{0, 10, 0}},
      Triangle{WorldPosition{50, 50, 50}, WorldPosition{60, 50, 50},
               WorldPosition{50, 60, 50}},
  };

  std::ostringstream out(std::ios::binary);
  std::string error;
  check(write_tri_stream(out, soup, error), "write succeeds");
  check(error.empty(), "no write error");

  std::istringstream in(out.str(), std::ios::binary);
  const TriLoadResult loaded = load_tri_stream(in);
  check(loaded.ok, "round-trip load succeeds");
  check(loaded.triangles.size() == 2, "triangle count preserved");
  check(loaded.triangles[0][1].x == 10.0f, "vertex preserved");
  check(loaded.triangles[1][2].z == 50.0f, "second triangle preserved");

  // The loaded soup drives the R11 BVH directly.
  const LosBvh bvh(loaded.triangles);
  check(bvh.segment_blocked(WorldPosition{1, 1, -5}, WorldPosition{1, 1, 5}),
        "loaded geometry blocks a crossing segment");
  check(!bvh.segment_blocked(WorldPosition{1, 1, 5}, WorldPosition{1, 1, 20}),
        "loaded geometry does not invent occlusion");
}

void test_rejection_paths() {
  // Bad magic.
  std::istringstream bad("XXXX\0\0\0\0\0\0\0\0"s, std::ios::binary);
  TriLoadResult result = load_tri_stream(bad);
  check(!result.ok && result.error.find("magic") != std::string::npos,
        "bad magic rejected");

  // Truncated payload: header promises two triangles, one is missing.
  std::ostringstream trunc(std::ios::binary);
  trunc.write("TRIZ", 4);
  const std::uint32_t version = 1;
  const std::uint32_t count = 2;
  trunc.write(reinterpret_cast<const char*>(&version), 4);
  trunc.write(reinterpret_cast<const char*>(&count), 4);
  const float vertex[9] = {0, 0, 0, 10, 0, 0, 0, 10, 0};
  trunc.write(reinterpret_cast<const char*>(vertex), 36);  // one of two
  std::istringstream trunc_in(trunc.str(), std::ios::binary);
  result = load_tri_stream(trunc_in);
  check(!result.ok && result.error.find("truncated") != std::string::npos,
        "truncated payload rejected");

  // Unsupported version.
  std::ostringstream future(std::ios::binary);
  future.write("TRIZ", 4);
  const std::uint32_t version_99 = 99;
  future.write(reinterpret_cast<const char*>(&version_99), 4);
  const std::uint32_t zero = 0;
  future.write(reinterpret_cast<const char*>(&zero), 4);
  std::istringstream future_in(future.str(), std::ios::binary);
  result = load_tri_stream(future_in);
  check(!result.ok && result.error.find("version") != std::string::npos,
        "future version rejected");

  // Non-finite coordinate.
  std::ostringstream nan_stream(std::ios::binary);
  nan_stream.write("TRIZ", 4);
  nan_stream.write(reinterpret_cast<const char*>(&version), 4);
  const std::uint32_t one = 1;
  nan_stream.write(reinterpret_cast<const char*>(&one), 4);
  const float nan_vertex[9] = {0, 0, 0, 10, 0, 0, 0, 10, 0.0f / 0.0f};
  nan_stream.write(reinterpret_cast<const char*>(nan_vertex), 36);
  std::istringstream nan_in(nan_stream.str(), std::ios::binary);
  result = load_tri_stream(nan_in);
  check(!result.ok && result.error.find("non-finite") != std::string::npos,
        "non-finite vertex rejected");
}

}  // namespace

int main() {
  test_round_trip_and_query();
  test_rejection_paths();

  if (failures != 0) {
    std::printf("%d tri loader check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_tri_loader_test: all checks passed\n");
  return 0;
}
