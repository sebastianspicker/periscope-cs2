#include "blue_example.hpp"

#include <cstdio>

namespace examples::entity_stream_crypto {

// BLUE: multi-reason detect key-exfil product, then strip client key + fog+.
BlueResult detect(sim::World& w) {
  BlueResult r;

  // Observe scars *before* mitigation mutates World.
  const bool encrypted = w.entity_stream_encrypted;
  const bool client_key = w.client_has_stream_key;
  const bool exfil = w.stream_key_exfiltrated;
  const bool full_origin = w.server_sends_full_enemy_origin;

  if (encrypted && client_key) {
    r.reasons.emplace_back("encrypted_stream_with_client_key");
  }
  if (exfil) {
    r.reasons.emplace_back("stream_key_exfiltrated");
  }
  if (full_origin) {
    r.reasons.emplace_back("full_enemy_origin_product");
  }

  bool foreign_vm = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* p = w.proc(h.owner_pid);
    if (p && !p->is_game && !p->is_ac) {
      foreign_vm = true;
      break;
    }
  }
  if (foreign_vm) {
    r.reasons.emplace_back("foreign_vm_read_key_thief_channel");
  }

  const bool specific_scar = client_key && exfil;
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: client stream key + exfil");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2 && specific_scar;

  // Mitigate: keep encryption, strip client key, apply fog+ budget.
  if (r.detected) {
    w.client_has_stream_key = false;
    w.stream_key_exfiltrated = false;
    w.entity_stream_encrypted = true;
    w.apply_client_fidelity_budget(0.0f, 0);
    r.mitigated = !w.client_has_stream_key && w.entity_stream_encrypted &&
                  !w.server_sends_full_enemy_origin;
    if (r.mitigated) w.ranked_access_denied = true;
  }

  r.detail = "entity_stream_crypto blue signals=" + std::to_string(r.signals) +
             " detected=" + std::to_string(r.detected ? 1 : 0) +
             " mitigated=" + std::to_string(r.mitigated ? 1 : 0) +
             " fidelity=" + std::to_string(w.client_entity_fidelity);
  w.note(r.detail);
  std::printf("[entity_stream_crypto] BLUE signals=%d detected=%s mitigated=%s\n",
              r.signals, r.detected ? "true" : "false",
              r.mitigated ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[entity_stream_crypto]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::entity_stream_crypto
