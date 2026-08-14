// crypto_offsets.hpp — implements CryptoOffsets (T1 red).
// Simulation unit; scars live on sim::World.

#pragma once

#include "ac/types.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace t1_red {

/// Educational stand-in for C2-delivered encrypted offset tables.
class CryptoOffsets {
 public:
  static std::vector<std::uint8_t> seal(const std::unordered_map<std::string, std::uint64_t>& map,
                                        std::uint8_t key);
  ac::Status open(const std::vector<std::uint8_t>& sealed, std::uint8_t key);
  std::uint64_t get(const std::string& k, std::uint64_t def = 0) const;

 private:
  std::unordered_map<std::string, std::uint64_t> map_;
};

}  // namespace t1_red
