// crypto_offsets.cpp — implements crypto_offsets (T1 red).
// Key methods: seal, open, get.

#include "t1_red/crypto_offsets.hpp"

namespace t1_red {

// CryptoOffsets::seal: Encrypt/seal offset blob for staged delivery (lab bytes only).
std::vector<std::uint8_t> CryptoOffsets::seal(
    const std::unordered_map<std::string, std::uint64_t>& map, std::uint8_t key) {
  std::vector<std::uint8_t> plain;
  for (const auto& [k, v] : map) {
    plain.push_back(static_cast<std::uint8_t>(k.size()));
    for (char c : k) {
      plain.push_back(static_cast<std::uint8_t>(c));
    }
    for (int b = 0; b < 8; ++b) {
      plain.push_back(static_cast<std::uint8_t>((v >> (8 * b)) & 0xff));
    }
  }
  for (auto& b : plain) {
    b = static_cast<std::uint8_t>(b ^ key);
  }
  return plain;
}

// CryptoOffsets::open: Open sealed offset blob into usable lab offsets.
ac::Status CryptoOffsets::open(const std::vector<std::uint8_t>& sealed,
                               std::uint8_t key) {
  map_.clear();
  std::vector<std::uint8_t> plain = sealed;
  for (auto& b : plain) {
    b = static_cast<std::uint8_t>(b ^ key);
  }
  std::size_t i = 0;
  while (i < plain.size()) {
    const auto n = plain[i++];
    if (i + n + 8 > plain.size()) {
      break;
    }
    std::string name(reinterpret_cast<const char*>(plain.data() + i), n);
    i += n;
    std::uint64_t v = 0;
    for (int b = 0; b < 8; ++b) {
      v |= static_cast<std::uint64_t>(plain[i++]) << (8 * b);
    }
    map_[name] = v;
  }
  return map_.empty() ? ac::Status::InvalidArgument : ac::Status::Ok;
}

// CryptoOffsets::get: Lookup a named offset/value from the loaded blob.
std::uint64_t CryptoOffsets::get(const std::string& k, std::uint64_t def) const {
  auto it = map_.find(k);
  return it == map_.end() ? def : it->second;
}

}  // namespace t1_red
