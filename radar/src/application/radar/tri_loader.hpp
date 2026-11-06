// tri_loader.hpp — R11 (portable half): map-collision `.tri` file format.
//
// Format (documented here; the extraction pipeline that produces the files
// from game assets is the lab/script half of ledger R11):
//
//   offset  size  field
//   0       4     magic "TRIZ"
//   4       4     u32 little-endian version (currently 1)
//   8       4     u32 little-endian triangle count N
//   12      36*N  N triangles as 9 float32 (x,y,z) * 3, CCW winding
//
// The loader accepts any std::istream so tests exercise it with
// std::stringstream and the future extraction script writes the same bytes;
// no filesystem access in the domain/application boundary. Decompressed
// `.tri.zst` content arrives through the stream, not through this code.

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "radar/los_bvh.hpp"

namespace radar_application {

struct TriLoadResult {
  bool ok = false;
  std::string error;                  // human-readable, empty when ok
  std::vector<radar_domain::Triangle> triangles;
};

// Reads a `.tri` stream (after any decompression). Rejects: bad magic,
// unsupported version, truncated data, non-finite coordinates, and a count
// field that would overflow the stream bounds.
TriLoadResult load_tri_stream(std::istream& stream);

// Writes the same format; round-tripping load(write(x)) == x is a test.
bool write_tri_stream(std::ostream& stream,
                      const std::vector<radar_domain::Triangle>& triangles,
                      std::string& error_out);

}  // namespace radar_application
