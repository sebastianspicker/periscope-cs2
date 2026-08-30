#include "red_example.hpp"

#include <cstdio>

namespace examples::entity_stream_crypto {

// RED: encrypted stream still free XY if client holds/exfils the key.
RedResult apply(sim::World& w) {
  RedResult r;
  int steps = 0;

  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    r.detail = "precondition failed: game process unavailable";
    return r;
  }
  ++steps;

  // Phase 1: actor + VM_READ channel (key thief needs process access).
  r.actor_pid = w.spawn("stream-key-thief.exe");
  if (!w.proc(r.actor_pid)) {
    r.detail = "actor spawn verification failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  if (!w.open_process(r.actor_pid, game_pid, sim::AccessMask::VmRead, false)) {
    r.detail = "OpenProcess VM_READ failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 2: structural crypto on entity stream (encrypted blobs).
  w.entity_stream_encrypted = true;
  w.server_sends_full_enemy_origin = true;
  ++steps;

  // Phase 3: client holds session key and exfils it — encryption alone fails.
  w.client_has_stream_key = true;
  w.stream_key_exfiltrated = true;
  r.has_key = true;
  ++steps;

  const bool scar_verified = w.entity_stream_encrypted && w.client_has_stream_key &&
                             w.stream_key_exfiltrated &&
                             w.server_sends_full_enemy_origin;
  if (!scar_verified) {
    r.detail = "scar verification failed: encrypted stream + client key exfil";
    r.steps = steps;
    return r;
  }
  ++steps;

  r.achieved = true;
  r.steps = steps;
  r.detail = "entity_stream_crypto red encrypted=1 client_key=1 exfil=1 full_origin=1 steps=" +
             std::to_string(steps);
  w.note(r.detail);
  std::printf("[entity_stream_crypto] RED achieved=true steps=%d\n", steps);
  return r;
}

}  // namespace examples::entity_stream_crypto
