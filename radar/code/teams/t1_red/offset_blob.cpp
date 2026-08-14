// offset_blob.cpp — implements offset_blob (T1 red).
// Key methods: load_encrypted, get.

#include "t1_red/offset_blob.hpp"

namespace t1_red {

// OffsetBlob::load_encrypted: Load encrypted offset payload into lab memory.
ac::Status OffsetBlob::load_encrypted(const std::vector<std::uint8_t>& blob,
                                      std::uint8_t key) {
  entries_.clear();
  if (blob.size() < 2) {
    return ac::Status::InvalidArgument;
  }
  std::vector<std::uint8_t> plain(blob.size());
  for (std::size_t i = 0; i < blob.size(); ++i) {
    plain[i] = static_cast<std::uint8_t>(blob[i] ^ key);
  }
  // Lab format: [u8 name_len][name...][u64 value] repeating
  std::size_t i = 0;
  while (i < plain.size()) {
    const auto nlen = plain[i++];
    if (i + nlen + 8 > plain.size()) {
      break;
    }
    Entry e;
    e.name.assign(reinterpret_cast<const char*>(plain.data() + i), nlen);
    i += nlen;
    std::uint64_t v = 0;
    for (int b = 0; b < 8; ++b) {
      v |= static_cast<std::uint64_t>(plain[i++]) << (8 * b);
    }
    e.value = v;
    entries_.push_back(std::move(e));
  }
  return entries_.empty() ? ac::Status::InvalidArgument : ac::Status::Ok;
}

// OffsetBlob::get: Lookup a named offset/value from the loaded blob.
std::uint64_t OffsetBlob::get(const std::string& name, std::uint64_t default_v) const {
  for (const auto& e : entries_) {
    if (e.name == name) {
      return e.value;
    }
  }
  return default_v;
}

}  // namespace t1_red
