// tri_loader.cpp — R11 `.tri` collision file format. See tri_loader.hpp.

#include "radar/tri_loader.hpp"

#include <cmath>
#include <cstring>
#include <istream>
#include <ostream>

namespace radar_application {
namespace {

constexpr char kMagic[4] = {'T', 'R', 'I', 'Z'};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kMaxTriangles = 10u * 1000u * 1000u;  // sanity bound

bool read_u32(std::istream& stream, std::uint32_t& out) {
  char bytes[4];
  if (!stream.read(bytes, 4)) {
    return false;
  }
  // The format is little-endian (tri_loader.hpp); assemble explicitly so the
  // decoder is host-endianness-independent instead of a memcpy that silently
  // misreads on big-endian hosts.
  out = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 8) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 16) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3])) << 24);
  return true;
}

bool read_float(std::istream& stream, float& out) {
  std::uint32_t bits = 0;
  if (!read_u32(stream, bits)) {
    return false;
  }
  std::memcpy(&out, &bits, 4);  // IEEE-754 bit pattern, now endian-correct
  return true;
}

bool write_u32(std::ostream& stream, std::uint32_t value) {
  const char bytes[4] = {
      static_cast<char>(value & 0xFFu),
      static_cast<char>((value >> 8) & 0xFFu),
      static_cast<char>((value >> 16) & 0xFFu),
      static_cast<char>((value >> 24) & 0xFFu),
  };
  return static_cast<bool>(stream.write(bytes, 4));
}

bool write_float(std::ostream& stream, float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, 4);
  return write_u32(stream, bits);
}

}  // namespace

TriLoadResult load_tri_stream(std::istream& stream) {
  TriLoadResult result;
  if (!stream) {
    result.error = "stream not readable";
    return result;
  }

  char magic[4] = {};
  if (!stream.read(magic, 4) || std::memcmp(magic, kMagic, 4) != 0) {
    result.error = "bad magic (expected TRIZ)";
    return result;
  }

  std::uint32_t version = 0;
  if (!read_u32(stream, version) || version != kVersion) {
    result.error = "unsupported version";
    return result;
  }

  std::uint32_t count = 0;
  if (!read_u32(stream, count)) {
    result.error = "truncated header";
    return result;
  }
  if (count > kMaxTriangles) {
    result.error = "triangle count exceeds sanity bound";
    return result;
  }

  result.triangles.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    radar_domain::Triangle triangle;
    for (radar_domain::WorldPosition& point : triangle) {
      if (!read_float(stream, point.x) || !read_float(stream, point.y) ||
          !read_float(stream, point.z)) {
        result.error = "truncated triangle data";
        return result;
      }
      if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
          !std::isfinite(point.z)) {
        result.error = "non-finite vertex coordinate";
        return result;
      }
    }
    result.triangles.push_back(triangle);
  }

  result.ok = true;
  return result;
}

bool write_tri_stream(std::ostream& stream,
                      const std::vector<radar_domain::Triangle>& triangles,
                      std::string& error_out) {
  if (triangles.size() > kMaxTriangles) {
    error_out = "triangle count exceeds sanity bound";
    return false;
  }
  if (!stream.write(kMagic, 4) || !write_u32(stream, kVersion) ||
      !write_u32(stream, static_cast<std::uint32_t>(triangles.size()))) {
    error_out = "stream write failed";
    return false;
  }
  for (const radar_domain::Triangle& triangle : triangles) {
    for (const radar_domain::WorldPosition& point : triangle) {
      if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
          !std::isfinite(point.z)) {
        error_out = "non-finite vertex coordinate";
        return false;
      }
      if (!write_float(stream, point.x) || !write_float(stream, point.y) ||
          !write_float(stream, point.z)) {
        error_out = "stream write failed";
        return false;
      }
    }
  }
  return true;
}

}  // namespace radar_application
