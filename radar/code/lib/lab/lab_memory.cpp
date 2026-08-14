// lab_memory.cpp — lab fixture process / memory backend for isolated unit tests.
// Provides attachable target_id.

#include "lab/lab_memory.hpp"

#include "lab/fixture_process.hpp"

#include <algorithm>
#include <cctype>

namespace lab {

std::vector<std::uint8_t> materialize_pattern_bytes(std::string_view hex) {
  std::vector<std::uint8_t> bytes;
  for (std::size_t pos = 0; pos < hex.size();) {
    while (pos < hex.size() &&
           std::isspace(static_cast<unsigned char>(hex[pos]))) {
      ++pos;
    }
    if (pos >= hex.size()) break;
    if (hex[pos] == '?') {
      ++pos;
      if (pos < hex.size() && hex[pos] == '?') ++pos;
      bytes.push_back(0);
      continue;
    }
    const auto hex_value = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    };
    if (pos + 1 >= hex.size()) return {};
    const int high = hex_value(hex[pos++]);
    const int low = hex_value(hex[pos++]);
    if (high < 0 || low < 0) return {};
    bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
  }
  return bytes;
}

void plant_cs2_pattern_markers(std::vector<std::uint8_t>& memory,
                               std::uint64_t,
                               const cs2::SignatureDatabase& db) {
  static constexpr std::string_view kNames[] = {
      "GETENTITYBYINDEX", "GETBASEENTITY", "GETLOCALPLAYERCONTROLLER",
      "GETLOCALPAWN", "GETPLAYERCONTROLLER", "GETENTITYHANDLE", "PENTITYLIST",
      "PENTITYSYSTEM", "PVIEWMATRIX", "PVIEWRENDER", "FINDENTITYBYCLASSNAME",
      "GETABSORIGIN", "GETBONEPOSITIONBYNAME", "GETTRANSFORMSFORHITBOXLIST",
      "CALCWORLDSPACEBONES", "C_BASEENTITY_GETBONEIDBYNAME",
      "C_BASEENTITY_GETHITBOXSET", "GETEYEANGLES", "GETVIEWANGLES", "PCLIENTMODE",
  };
  constexpr std::size_t kMarkerStart = 0x4E0000;
  constexpr std::size_t kMarkerGap = 16;
  std::size_t offset = kMarkerStart;
  for (const auto name : kNames) {
    const auto* pattern = db.find(name);
    if (!pattern) continue;
    const auto bytes = materialize_pattern_bytes(pattern->bytes_hex);
    if (bytes.empty()) continue;
    if (memory.size() < offset + bytes.size()) {
      memory.resize(offset + bytes.size(), 0);
    }
    std::copy(bytes.begin(), bytes.end(),
              memory.begin() + static_cast<std::ptrdiff_t>(offset));
    offset += bytes.size() + kMarkerGap;
  }
}

void LabMemoryBackend::reset_stats() {
  stats_ = {};
  have_last_read_ = false;
  last_read_addr_ = 0;
}

ac::Status LabMemoryBackend::attach(std::uint32_t target_id) {
  // Accept global fixture or any registered fixture whose id matches exactly.
  auto& resolved = fixture_by_id(target_id);
  if (resolved.id() != target_id) {
    return ac::Status::Unavailable;
  }
  target_id_ = target_id;
  attached_ = true;
  reset_stats();
  return ac::Status::Ok;
}

void LabMemoryBackend::detach() {
  attached_ = false;
  target_id_ = 0;
  have_last_read_ = false;
}

ac::ReadResult LabMemoryBackend::read(const ac::ReadRequest& req) {
  if (!attached_) {
    return ac::ReadResult{ac::Status::Unavailable, {}};
  }
  auto& fx = fixture_by_id(target_id_);
  auto out = fx.read_bytes(req.address, req.size);
  if (out.status == ac::Status::Ok) {
    ++stats_.read_ops;
    stats_.read_bytes += out.bytes.size();
    if (stats_.read_bytes >= (1u << 20) || stats_.read_ops >= 4) {
      stats_.bulk_read = true;
    }
    if (have_last_read_) {
      const auto delta = req.address > last_read_addr_
                             ? req.address - last_read_addr_
                             : last_read_addr_ - req.address;
      if (delta <= 0x1000ull && stats_.read_ops >= 4) {
        stats_.sequential_burst = true;
      }
    }
    last_read_addr_ = req.address;
    have_last_read_ = true;
  }
  return out;
}

ac::Status LabMemoryBackend::write(std::uint64_t address,
                                   std::span<const std::uint8_t> data) {
  if (!attached_) return ac::Status::Unavailable;
  auto& fx = fixture_by_id(target_id_);
  fx.write_bytes(address, data);
  ++stats_.write_ops;
  stats_.write_bytes += data.size();
  return ac::Status::Ok;
}

ScatterReadResult LabMemoryBackend::scatter_read(
    const std::vector<ScatterReadRequest>& reqs) {
  ScatterReadResult result;
  if (!attached_) {
    result.status = ac::Status::Unavailable;
    return result;
  }
  ++stats_.scatter_ops;
  result.parts.reserve(reqs.size());
  for (const auto& r : reqs) {
    auto part = read(ac::ReadRequest{r.address, r.size});
    if (part.status != ac::Status::Ok) {
      result.status = part.status;
    }
    result.parts.push_back(std::move(part));
  }
  return result;
}

}  // namespace lab
