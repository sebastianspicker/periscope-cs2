#include "red_example.hpp"
#include "server/info_advantage.hpp"

#include <cstdio>

namespace examples::info_advantage {

// RED: legit radar — human aim with unfair hidden knowledge (no aimbot write).
// Plants World.aim_samples residuals blue must score (not invent).
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

  r.actor_pid = w.spawn("legit-radar-player.exe");
  if (!w.proc(r.actor_pid)) {
    r.detail = "actor spawn verification failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 1: multi-frame pre-aim on hidden targets (no vision/audio).
  for (int i = 0; i < 5; ++i) {
    sim::AimSample s;
    s.camera_yaw = 10.f + static_cast<float>(i);
    s.server_aim_yaw = 10.f + static_cast<float>(i);  // human aim, not silent desync
    s.aim_on_hidden_target = true;
    s.has_vision_on_target = false;
    s.has_audio_on_target = false;
    s.challenge_passed = true;
    w.aim_samples.push_back(s);
  }
  if (static_cast<int>(w.aim_samples.size()) < 5) {
    r.detail = "pre-aim sample plant failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 2: second residual channel — spectator/delayed feed (optional depth).
  w.spectator_feed_active = true;
  w.spectator_has_delayed_enemy_origin = true;
  w.spectator_count = 1;
  ++steps;

  server::InfoAdvantageScorer sc;
  for (const auto& s : w.aim_samples) {
    server::DemoFrame f;
    f.aim_on_hidden_target = s.aim_on_hidden_target;
    f.has_vision_on_target = s.has_vision_on_target;
    f.has_audio_on_target = s.has_audio_on_target;
    sc.on_frame(f);
  }
  auto score = sc.result();
  r.preaim_frames = score.hits;
  r.score = score.score;
  r.achieved = score.hits >= 3 && static_cast<int>(w.aim_samples.size()) >= 3 &&
               w.spectator_feed_active;
  r.steps = steps;
  r.detail = "info_advantage red preaim_hits=" + std::to_string(score.hits) +
             " samples=" + std::to_string(w.aim_samples.size()) +
             " spectator=1 score=" + std::to_string(score.score) +
             " steps=" + std::to_string(steps);
  w.note(r.detail);
  std::printf("[info_advantage] RED achieved=%s steps=%d\n",
              r.achieved ? "true" : "false", steps);
  return r;
}

}  // namespace examples::info_advantage
