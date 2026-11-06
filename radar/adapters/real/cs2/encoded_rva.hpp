// encoded_rva.hpp — compile-time encoded RVA table (E59).
//
// The pinned snapshot's globals are stored XOR-encoded so the plaintext RVAs
// are not the obvious static constants a signature tool greps for. The key is
// a build-stable constant; `rva_decode` is constexpr so every use folds back
// to the same value at compile time.
//
// Strings and AOB patterns were already obfuscated (`win/xorstr.hpp`); this
// closes the remaining plaintext surface. Field *offsets* stay plaintext —
// they are small numbers and carry no address meaning on their own.

#pragma once

#include <cstdint>

namespace real::cs2::encoded {

inline constexpr std::uint64_t kRvaKey = 0x9E3779B97F4A7C15ull;

constexpr std::uint64_t encode(std::uint64_t plain) { return plain ^ kRvaKey; }
constexpr std::uint64_t decode(std::uint64_t encoded) {
  return encoded ^ kRvaKey;
}

}  // namespace real::cs2::encoded
