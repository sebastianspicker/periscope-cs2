#include "blue_example.hpp"
#include "server/info_advantage.hpp"

#include <cstdio>

namespace examples::info_advantage {

// BLUE: score World residuals only (aim_samples + optional spectator flags).
// Never invent DemoFrames on a clean arena.
BlueResult detect(sim::World& w) {
  BlueResult r;
  server::InfoAdvantageScorer sc;

  int hidden_preaim = 0;
  int no_vision = 0;
  for (const auto& s : w.aim_samples) {
    server::DemoFrame f;
    f.aim_on_hidden_target = s.aim_on_hidden_target;
    f.has_vision_on_target = s.has_vision_on_target;
    f.has_audio_on_target = s.has_audio_on_target;
    f.aim_yaw = s.server_aim_yaw;
    sc.on_frame(f);
    if (s.aim_on_hidden_target) ++hidden_preaim;
    if (s.aim_on_hidden_target && !s.has_vision_on_target && !s.has_audio_on_target) {
      ++no_vision;
    }
  }

  // Spectator / delayed-replay residual is a World flag, not invented frames.
  if (w.spectator_feed_active && w.spectator_has_delayed_enemy_origin) {
    server::DemoFrame f;
    f.aim_on_hidden_target = true;
    f.has_vision_on_target = false;
    f.has_audio_on_target = false;
    sc.on_frame(f);
    r.reasons.emplace_back("spectator_delayed_enemy_origin");
  }

  auto s = sc.result();
  r.score = s.score;

  if (hidden_preaim >= 3) {
    r.reasons.emplace_back("hidden_preaim_frames=" + std::to_string(hidden_preaim));
  }
  if (no_vision >= 3) {
    r.reasons.emplace_back("preaim_without_vision_or_audio n=" +
                           std::to_string(no_vision));
  }
  if (static_cast<int>(w.aim_samples.size()) >= 3) {
    r.reasons.emplace_back("aim_sample_count=" +
                           std::to_string(w.aim_samples.size()));
  }
  if (s.multi_reason || s.reasons.size() >= 2) {
    for (const auto& sr : s.reasons) {
      r.reasons.emplace_back("scorer:" + sr);
    }
  }

  const bool specific_scar = s.hits >= 3;
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: multi-frame hidden pre-aim");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.multi_reason = r.signals >= 2 || s.multi_reason ||
                   (s.hits >= 3 && w.spectator_feed_active);
  r.detected = r.signals >= 2 && specific_scar;
  if (r.detected) {
    r.mitigated = r.multi_reason || s.hits >= 4;
    if (r.mitigated) {
      w.ranked_access_denied = true;
      w.overwatch_queued = true;
    }
  }

  r.detail = "info_advantage blue hits=" + std::to_string(s.hits) +
             " samples=" + std::to_string(w.aim_samples.size()) +
             " multi=" + std::to_string(r.multi_reason ? 1 : 0) +
             " spectator=" + std::to_string(w.spectator_feed_active ? 1 : 0) +
             " signals=" + std::to_string(r.signals) +
             " detected=" + std::to_string(r.detected ? 1 : 0);
  w.note(r.detail);
  std::printf("[info_advantage] BLUE signals=%d detected=%s\n", r.signals,
              r.detected ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[info_advantage]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::info_advantage
