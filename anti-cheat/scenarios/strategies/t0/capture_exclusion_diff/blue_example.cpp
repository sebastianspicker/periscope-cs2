#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::capture_exclusion_diff {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = 1;
    return result;
  }

  const auto sig = [&result](bool present, std::string reason) {
    strategies::add_signal(result.signals, result.reasons, present,
                           reason.c_str());
  };

  // Reason 1: the capture sensor is armed.
  w.capture_sensor_active = true;
  sig(w.capture_sensor_active, "capture-diff sensor armed");

  // Reason 2: the capture does not contain the overlay at all.
  sig(!w.capture_sees_overlays, "capture path does not see overlay pixels");

  // Reason 3: the frame diff is empty although the presenter is running.
  sig(w.capture_frame_diff_pixels == 0,
      "captured-vs-presented diff pixels=" +
          std::to_string(w.capture_frame_diff_pixels));

  // Reason 4: the exclusion flag itself is set on the window.
  sig(w.wda_excluded_from_capture,
      "WDA_EXCLUDEFROMCAPTURE flag present (updates=" +
          std::to_string(w.wda_exclude_count) + ")");

  const bool specific_scar =
      !w.capture_sees_overlays && w.capture_frame_diff_pixels == 0 &&
      w.wda_excluded_from_capture;
  if (specific_scar) {
    w.capture_exclusion_observed = true;
    result.reasons.emplace_back(
        "strategy scar: overlay present for the player, absent from capture");
  }

  // Mitigation: require an out-of-band heartbeat (a second capture device or
  // a signed window list) so an excluded overlay cannot be the only evidence.
  if (result.detected || specific_scar) {
    result.reasons.emplace_back(
        "mitigation: out-of-band overlay heartbeat required");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && result.risk >= 0.65;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "capture_exclusion_diff blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf(
      "[T0 capture_exclusion_diff] BLUE signals=%d detected=%s mitigated=%s\n",
      result.signals, result.detected ? "true" : "false",
      result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T0 capture_exclusion_diff]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::capture_exclusion_diff
