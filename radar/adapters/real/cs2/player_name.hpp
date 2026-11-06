// player_name.hpp — sanitise a game-supplied player name (E50).
//
// Names come from a remote string and are attacker-controlled. The reader
// caps the read at 127 bytes; this helper removes control bytes, collapses
// whitespace and trims, so a name can be drawn without a second pass.
//
// Pure and host-testable.

#pragma once

#include <cstddef>
#include <string>

namespace real::cs2::name {

inline std::string sanitize(const char* raw, std::size_t max_len = 127) {
  if (raw == nullptr) return {};
  std::string out;
  out.reserve(max_len < 32 ? max_len : 32);
  bool last_space = true;
  for (std::size_t i = 0; i < max_len && raw[i] != '\0'; ++i) {
    const unsigned char c = static_cast<unsigned char>(raw[i]);
    if (c < 0x20 || c == 0x7F) {
      if (!out.empty() && !last_space) {
        out.push_back(' ');
        last_space = true;
      }
      continue;
    }
    if (c == ' ' && out.empty()) continue;
    out.push_back(static_cast<char>(c));
    last_space = (c == ' ');
    if (out.size() >= max_len) break;
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

}  // namespace real::cs2::name
