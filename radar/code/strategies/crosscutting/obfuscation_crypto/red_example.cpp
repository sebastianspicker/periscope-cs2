#include "red_example.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace examples::obfuscation_crypto {

RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("offset-cache-lab.exe");

  // This is an in-memory teaching sample, not a cryptographic implementation.
  std::array<std::uint8_t, 4> offset_blob{0x34, 0x12, 0x00, 0x00};
  constexpr std::uint8_t kDemoKey = 0x5A;
  for (auto& byte : offset_blob) byte ^= kDemoKey;
  for (auto& byte : offset_blob) byte ^= kDemoKey;

  w.entity_stream_encrypted = true;
  w.client_has_stream_key = true;
  w.add_section({"lab.encrypted-offset-cache", r.actor_pid, w.game_pid(), false});
  r.achieved = offset_blob[0] == 0x34;
  r.detail = "encrypted offset-cache artifact and protected stream metadata recorded";
  w.note(r.detail);
  return r;
}

}  // namespace examples::obfuscation_crypto
