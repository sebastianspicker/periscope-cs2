#include "blue_example.hpp"
#include "server/interest_mgmt.hpp"

#include <cstdio>

namespace examples::interest_mgmt {

// BLUE: multi-reason detect full-origin product; structural kill via interest fog+.
BlueResult detect(sim::World& w) {
  BlueResult r;

  // Observe product scars before fog mutates World.
  const bool full_origin = w.server_sends_full_enemy_origin;
  const bool client_key = w.client_has_stream_key;
  const bool exfil = w.stream_key_exfiltrated;
  const bool high_fidelity = w.client_entity_fidelity >= 1.0f;

  bool foreign_vm = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* p = w.proc(h.owner_pid);
    if (p && !p->is_game && !p->is_ac) {
      foreign_vm = true;
      break;
    }
  }

  if (full_origin && high_fidelity) {
    r.reasons.emplace_back("full_enemy_origin_product fidelity=1");
  }
  if (client_key) {
    r.reasons.emplace_back("client_has_stream_key");
  }
  if (exfil) {
    r.reasons.emplace_back("stream_key_exfiltrated");
  }
  if (foreign_vm) {
    r.reasons.emplace_back("foreign_vm_read_radar_channel");
  }

  server::InterestManager im;
  server::Observer obs{{0, 0, 0}, 0};
  std::vector<server::WorldEntity> all = {
      {1, {5, 0, 5}, 1, true},
      {2, {200, 0, 200}, 2, true},  // far enemy — should cull
      {3, {8, 0, 8}, 1, true},
  };
  server::InterestFilterStats st{};
  auto rep = im.filter_for_client_stats(obs, 1, all, 50.f, st);
  (void)rep;
  r.enemies_culled = st.enemies_culled;
  r.replicated = st.replicated;

  // Fog+: deny full origin product even if red has VmRead.
  w.apply_client_fidelity_budget(0.25f, /*enemy_budget=*/1);
  w.stream_key_exfiltrated = false;
  w.client_has_stream_key = false;
  w.entity_stream_encrypted = true;

  const bool fog_ok = !w.server_sends_full_enemy_origin &&
                      w.client_entity_fidelity < 1.0f && st.enemies_culled >= 1;
  if (fog_ok) {
    r.reasons.emplace_back("interest_fog_applied culled=" +
                           std::to_string(st.enemies_culled));
  }

  const bool specific_scar = full_origin && (client_key || foreign_vm);
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: full-origin client radar product");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2 && specific_scar;
  r.mitigated = fog_ok;
  if (r.detected && r.mitigated) {
    w.ranked_access_denied = true;
  }

  r.detail = "interest_mgmt blue fog+ culled=" + std::to_string(st.enemies_culled) +
             " fidelity=" + std::to_string(w.client_entity_fidelity) +
             " signals=" + std::to_string(r.signals) +
             " detected=" + std::to_string(r.detected ? 1 : 0) +
             " " + st.detail;
  w.note(r.detail);
  std::printf("[interest_mgmt] BLUE signals=%d detected=%s mitigated=%s\n",
              r.signals, r.detected ? "true" : "false",
              r.mitigated ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[interest_mgmt]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::interest_mgmt
