// offset_blob.hpp — implements OffsetBlob, Entry (T1 red).
// Simulation unit; scars live on sim::World.

#pragma once

#include "ac/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t1_red {

/// Encrypted-at-rest offset table surface (C2-delivered in real packs).
class OffsetBlob {
 public:
  /// Lab: "decrypt" is XOR with a fixed key — documents the surface only.
  ac::Status load_encrypted(const std::vector<std::uint8_t>& blob, std::uint8_t key);
  std::uint64_t get(const std::string& name, std::uint64_t default_v = 0) const;

 private:
// Entry: lab type for this educational unit.
  struct Entry {
    std::string name;
    std::uint64_t value = 0;
  };
  std::vector<Entry> entries_;
};

}  // namespace t1_red
