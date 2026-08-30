// BLUE: multi-reason handle scar + CS interest fog (far enemies not replicated).

#include "blue_example.hpp"

#include "fps/lab_bridge.hpp"
#include "server/interest_mgmt.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>

namespace examples::cs_round_radar {
namespace {

bool foreign_vm_read(const sim::World& w) {
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) {
      continue;
    }
    const auto* p = w.proc(h.owner_pid);
    if (p && !p->is_game && !p->is_ac) {
      return true;
    }
  }
  return false;
}

BlueResult fog_from_entities(sim::World& w,
                             const std::vector<server::WorldEntity>& truth,
                             const server::Observer& obs,
                             std::uint8_t observer_team) {
  BlueResult r;
  r.foreign_vm_read = foreign_vm_read(w);

  // Independent reason: foreign VM_READ channel to game.
  if (r.foreign_vm_read) {
    r.reasons.emplace_back("foreign_vm_read channel to game");
  }

  // Independent reason: remote read telemetry from entity product pull.
  if (w.remote_read_ops > 0 || w.remote_read_bytes > 0) {
    r.reasons.emplace_back("remote_read telemetry ops=" +
                           std::to_string(w.remote_read_ops) +
                           " bytes=" + std::to_string(w.remote_read_bytes));
  }

  // Independent reason: full-origin client product pressure.
  if (w.server_sends_full_enemy_origin && w.client_entity_fidelity >= 1.0f) {
    r.reasons.emplace_back("full_enemy_origin client product fidelity=1");
  }

  // Independent reason: entity truth table non-empty (RPM product).
  if (static_cast<int>(truth.size()) >= 2) {
    r.reasons.emplace_back("entity_truth_count=" +
                           std::to_string(truth.size()));
  }

  server::InterestManager im;
  server::InterestFilterStats st{};
  auto rep =
      im.filter_for_client_stats(obs, observer_team, truth, /*enemy_radius=*/50.f, st);
  (void)rep;
  r.enemies_culled = st.enemies_culled;
  r.truth_alive = st.truth_alive;
  r.replicated = st.replicated;

  // Structural fog+: starve full enemy origins even if red RPM succeeded.
  w.apply_client_fidelity_budget(0.25f, /*enemy_budget=*/1);
  r.fog = !w.server_sends_full_enemy_origin && w.client_entity_fidelity < 1.0f &&
          st.enemies_culled >= 1;

  if (r.fog) {
    r.reasons.emplace_back("interest_fog culled=" +
                           std::to_string(r.enemies_culled) +
                           " fidelity=" + std::to_string(w.client_entity_fidelity));
  } else if (st.enemies_culled >= 1) {
    r.reasons.emplace_back("interest_cull enemies_culled=" +
                           std::to_string(st.enemies_culled));
  }

  const bool specific_scar = r.foreign_vm_read;
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: foreign VM_READ CS entity product");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2 && specific_scar;
  r.mitigated = r.fog;
  if (r.detected && r.mitigated) {
    w.ranked_access_denied = true;
  }

  std::ostringstream oss;
  oss << "cs_round_radar blue handle=" << (r.foreign_vm_read ? 1 : 0)
      << " culled=" << r.enemies_culled << " truth=" << r.truth_alive
      << " rep=" << r.replicated
      << " fidelity=" << w.client_entity_fidelity
      << " signals=" << r.signals
      << " detected=" << (r.detected ? 1 : 0)
      << " " << st.detail;
  r.detail = oss.str();
  w.note(r.detail);
  std::printf("[cs_round_radar] BLUE signals=%d detected=%s fog=%s\n", r.signals,
              r.detected ? "true" : "false", r.fog ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[cs_round_radar]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace

BlueResult detect_with_scenario(sim::World& w, const fps::Scenario& sc) {
  std::vector<server::WorldEntity> truth;
  for (const auto& p : sc.players()) {
    if (!p.alive) {
      continue;
    }
    truth.push_back(server::WorldEntity{
        p.id, p.origin, static_cast<std::uint8_t>(p.team), p.alive});
  }
  // CT observer at CT spawn — T-spawn enemies are far → cull (CS fog lesson).
  const auto* ct = sc.map().find_area(fps::AreaId::CTSpawn);
  server::Observer obs{};
  obs.origin = ct ? ct->center : ac::Vec3{0.f, 0.f, 80.f};
  return fog_from_entities(w, truth, obs, /*observer_team=*/1);
}

BlueResult detect(sim::World& w) {
  // Rebuild entity truth from World memory if scenario not available.
  std::vector<server::WorldEntity> truth;
  auto* g = w.proc(w.game_pid());
  if (g && g->memory.size() >= 4) {
    std::uint32_t count = 0;
    std::memcpy(&count, g->memory.data(), sizeof(count));
    struct EntRaw {
      float x, y, z;
      std::uint8_t team, alive, pad[2];
    };
    for (std::uint32_t i = 0; i < count && i < 32; ++i) {
      EntRaw e{};
      const auto off = 0x10 + i * sizeof(EntRaw);
      if (off + sizeof(EntRaw) > g->memory.size()) {
        break;
      }
      std::memcpy(&e, g->memory.data() + off, sizeof(e));
      if (!e.alive) {
        continue;
      }
      truth.push_back(
          server::WorldEntity{i + 1, {e.x, e.y, e.z}, e.team, true});
    }
  }
  // Default CT-side observer (far from T spawn on dusty_yard).
  server::Observer obs{{0.f, 0.f, 80.f}, 0.f};
  return fog_from_entities(w, truth, obs, 1);
}

}  // namespace examples::cs_round_radar
