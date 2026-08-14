// RED example implementation for this strategy pair.
// Multi-step World scars for learners; pairs with blue_example detect/mitigate.

#include "red_example.hpp"

#include <cmath>
#include <cstdio>

namespace examples::speedhack_timescale {

namespace {
constexpr double kSpeedhackScale = 1.5;
constexpr double kEpsilon = 1e-3;
}  // namespace

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();

  std::printf("[red:speedhack_timescale] verify a game process is present\n");
  if (game == 0 || w.proc(game) == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:speedhack_timescale] spawn a lab speedhack actor\n");
  const auto actor = w.spawn("speedhack-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    r.detail = "lab actor process creation failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:speedhack_timescale] set QPC/tick time_scale to %.2f\n",
              kSpeedhackScale);
  w.time_scale = kSpeedhackScale;
  if (std::abs(w.time_scale - kSpeedhackScale) > kEpsilon) {
    r.detail = "could not plant time_scale scar";
    return r;
  }
  ++r.steps;

  std::printf("[red:speedhack_timescale] mark host trust timing as spoofed\n");
  w.trust.timing_spoofed = true;
  if (!w.trust.timing_spoofed) {
    r.detail = "could not plant timing_spoofed scar";
    return r;
  }
  ++r.steps;

  std::printf("[red:speedhack_timescale] verify scale deviation from 1.0\n");
  const double delta = std::abs(w.time_scale - 1.0);
  if (delta <= kEpsilon) {
    r.detail = "time_scale still near identity; speedhack not verified";
    return r;
  }
  ++r.steps;

  r.achieved = delta > kEpsilon && w.trust.timing_spoofed;
  r.detail = r.achieved
                 ? "time_scale speedhack and timing_spoofed residual verified"
                 : "speedhack timescale incomplete";
  w.note(r.detail);
  std::printf("[red:speedhack_timescale] %s (%d steps, delta=%.4f)\n",
              r.detail.c_str(), r.steps, delta);
  return r;
}

}  // namespace examples::speedhack_timescale
