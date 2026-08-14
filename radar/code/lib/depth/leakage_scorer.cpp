// leakage_scorer.cpp — depth analytics: multi-sample / fusion scoring for blue.
// Educational scorers over World residuals.

#include "depth/leakage_scorer.hpp"

#include <cmath>
#include <sstream>

namespace depth {
namespace {

// dist2: free function for this educational unit.
float dist2(const ac::Vec3& a, const ac::Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
}

}  // namespace

// LeakageScorer::replicate: Score how much unobservable state is replicated to client.
std::vector<ReplicatedEntity> LeakageScorer::replicate(
    const server::Observer& obs, std::uint8_t team,
    const std::vector<EntityTruth>& all) const {
  std::vector<ReplicatedEntity> out;
  const float r2 = radius_ * radius_;
  int far_enemy_idx = 0;

  for (const auto& e : all) {
    if (!e.alive) {
      continue;
    }
    ReplicatedEntity r;
    r.id = e.id;
    r.origin = e.origin;
    r.team = e.team;
    r.alive = e.alive;

    if (e.team == team) {
      out.push_back(r);
      continue;
    }

    const bool in_radius = dist2(obs.origin, e.origin) <= r2;
    const bool observable = e.currently_observable || in_radius;

    switch (fog_) {
      case FogPolicy::FullReplication:
        out.push_back(r);
        break;
      case FogPolicy::StrictRadius:
        if (observable) {
          out.push_back(r);
        }
        break;
      case FogPolicy::PartialLeak: {
        if (observable) {
          out.push_back(r);
        } else {
          // Deterministic leak of a fraction of far enemies (buggy fog).
          ++far_enemy_idx;
          const float gate = partial_leak_;
          // Leak every 1/gate-th far entity by index hash.
          if (gate > 0.f &&
              (static_cast<float>((far_enemy_idx * 17) % 100) / 100.f) < gate) {
            r.leaked_beyond_fog = true;
            out.push_back(r);
          }
        }
        break;
      }
      case FogPolicy::DelayedOrigin:
        if (observable) {
          out.push_back(r);
        } else {
          // Delayed: send stale offset origin (still useful for rough radar).
          r.delayed = true;
          r.origin.x -= 5.f * static_cast<float>(delay_ticks_);
          r.origin.z -= 5.f * static_cast<float>(delay_ticks_);
          out.push_back(r);
        }
        break;
    }
  }
  return out;
}

// LeakageScorer::score: Compute current multi-signal risk from ingested events.
LeakageScore LeakageScorer::score(const server::Observer& obs, std::uint8_t team,
                                  const std::vector<EntityTruth>& all,
                                  const StreamCryptoState& stream) const {
  LeakageScore s;
  const float r2 = radius_ * radius_;
  int far_truth = 0;

  for (const auto& e : all) {
    if (!e.alive || e.team == team) {
      continue;
    }
    ++s.truth_enemies;
    if (dist2(obs.origin, e.origin) > r2 && !e.currently_observable) {
      ++far_truth;
    }
  }

  auto rep = replicate(obs, team, all);
  for (const auto& e : rep) {
    if (e.team == team || !e.alive) {
      continue;
    }
    ++s.replicated_enemies;
    if (e.leaked_beyond_fog) {
      ++s.leaked_beyond_fog;
    }
    if (e.delayed) {
      ++s.delayed_origins;
    }
  }

  if (far_truth > 0) {
    s.leakage_ratio =
        static_cast<double>(s.leaked_beyond_fog + s.delayed_origins) /
        static_cast<double>(far_truth);
  }

  // Stream crypto dynamics
  if (stream.key_exfiltrated || (stream.encrypted && stream.client_has_key)) {
    s.key_exfil_events = stream.key_exfiltrated ? 1 : 0;
    s.risk += 2.0;  // plaintext or decryptable XY still available
  }
  if (stream.encrypted && !stream.client_has_key && !stream.key_exfiltrated) {
    s.risk -= 1.5;  // ciphertext without key
  }

  s.risk += s.leakage_ratio * 4.0;
  s.risk += static_cast<double>(s.replicated_enemies) * 0.25;
  if (fog_ == FogPolicy::FullReplication) {
    s.risk += 3.0;
  }

  // Structural kill: low leakage + no usable key path
  const bool no_key =
      stream.encrypted && !stream.client_has_key && !stream.key_exfiltrated;
  const bool tight_fog =
      fog_ == FogPolicy::StrictRadius && s.leaked_beyond_fog == 0 &&
      s.delayed_origins == 0;
  s.structural_kill = (tight_fog || (fog_ != FogPolicy::FullReplication &&
                                     s.leakage_ratio < 0.15)) &&
                      (no_key || !stream.encrypted);

  // If not encrypted but strict fog, still structural kill for far radar.
  if (tight_fog && fog_ == FogPolicy::StrictRadius) {
    s.structural_kill = true;
  }

  std::ostringstream oss;
  oss << "fog=" << static_cast<int>(fog_) << " truth_e=" << s.truth_enemies
      << " repl_e=" << s.replicated_enemies << " leak=" << s.leaked_beyond_fog
      << " delayed=" << s.delayed_origins << " ratio=" << s.leakage_ratio
      << " key_exfil=" << s.key_exfil_events << " risk=" << s.risk
      << " kill=" << (s.structural_kill ? 1 : 0);
  s.detail = oss.str();
  return s;
}

// LeakageScorer::mitigate_world: Apply leakage mitigate (fog / drop full origin).
LeakageScore LeakageScorer::mitigate_world(
    sim::World& w, const server::Observer& obs, std::uint8_t team,
    const std::vector<EntityTruth>& all) const {
  // Multi-step blue structural kill:
  //  1) drop full-replication origin stream
  //  2) force strict interest radius (no partial/delayed far leak)
  //  3) encrypt entity stream + rotate/strip client key
  //  4) clear any prior key-exfil scar
  w.server_sends_full_enemy_origin = false;
  w.entity_stream_encrypted = true;
  w.client_has_stream_key = false;
  w.stream_key_exfiltrated = false;

  LeakageScorer hard = *this;
  hard.set_fog(FogPolicy::StrictRadius);
  hard.set_partial_leak_fraction(0.f);
  hard.set_delay_ticks(0);
  StreamCryptoState st;
  st.encrypted = true;
  st.client_has_key = false;
  st.key_exfiltrated = false;
  st.server_rotated_key = true;
  auto s = hard.score(obs, team, all, st);
  // Structural kill must hold after full multi-step policy.
  if (!s.structural_kill && s.leaked_beyond_fog == 0 &&
      s.delayed_origins == 0 && !st.client_has_key && !st.key_exfiltrated) {
    s.structural_kill = true;
    s.detail += " force_kill=1";
  }
  w.note("leakage_mitigate " + s.detail);
  return s;
}

// run_stream_exfil_red: free function for this educational unit.
StreamExfilResult run_stream_exfil_red(sim::World& w) {
  StreamExfilResult r;
  w.server_sends_full_enemy_origin = true;
  r.wanted_full_replication = true;

  // Product may encrypt; red hopes key is still in client process.
  if (!w.entity_stream_encrypted) {
    w.entity_stream_encrypted = true;  // observe encrypted path
  }
  if (w.client_has_stream_key) {
    r.key_present = true;
    w.stream_key_exfiltrated = true;
    r.key_exfiltrated = true;
  } else {
    // Attempt exfil scar: plant key then steal (lab residual).
    w.client_has_stream_key = true;
    r.key_present = true;
    w.stream_key_exfiltrated = true;
    r.key_exfiltrated = true;
  }

  r.useful_radar =
      w.server_sends_full_enemy_origin || w.stream_key_exfiltrated;
  r.detail = std::string("full_repl=") +
             (r.wanted_full_replication ? "1" : "0") +
             " key_exfil=" + (r.key_exfiltrated ? "1" : "0");
  w.note("stream_exfil_red " + r.detail);
  return r;
}

}  // namespace depth
