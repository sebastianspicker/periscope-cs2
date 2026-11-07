// stack_posture_test.cpp: host test for the pure intel/read posture policy.
//
// Covers the two ways this policy can quietly go wrong:
//   1. The per-frame event shaper overwriting a degrade (so a ladder step never
//      survives), and
//   2. A pinned intel policy being silently overridden.
// Both are checked without Windows, threads, or the real clock domain.

#include "real/cs2/posture.hpp"

#include <chrono>
#include <cstdio>

namespace {

using namespace real::cs2::stack;
using Clock = std::chrono::steady_clock;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

constexpr ReadRates kFloor{8, 15, 4};

RoundEvents bomb_planted() {
  RoundEvents ev;
  ev.valid = true;
  ev.is_round = true;
  ev.bomb_planted = true;
  return ev;
}

RoundEvents quiet_round() {
  RoundEvents ev;
  ev.valid = true;
  ev.is_round = true;
  return ev;
}

// ── Shaper ───────────────────────────────────────────────────────
void test_quiet_floor() {
  BehaviorShaper shaper;
  const auto now = Clock::now();
  const auto rates = shaper.rates_at(now, kFloor);
  check(rates.full_hz == kFloor.full_hz, "quiet floor keeps full rate");
  check(rates.local_hz == kFloor.local_hz, "quiet floor keeps local rate");
  check(rates.far_hz == kFloor.far_hz, "quiet floor keeps far rate");
}

void test_event_burst_is_bounded() {
  BehaviorShaper shaper;
  const auto now = Clock::now();
  shaper.note_round(bomb_planted(), 0.f, now);
  check(shaper.surging(now), "plant starts a surge");
  const auto rates = shaper.rates_at(now, kFloor);
  check(rates.full_hz > kFloor.full_hz, "surge raises the full rate");
  check(rates.full_hz <= 30, "surge stays under the historical full cap");
  check(rates.local_hz <= 45, "surge stays under the historical local cap");
  check(rates.far_hz <= 8, "surge stays under the historical far cap");
}

void test_burst_expires() {
  BehaviorShaper shaper;
  const auto now = Clock::now();
  shaper.note_round(bomb_planted(), 0.f, now);
  const auto later = now + std::chrono::seconds(5);
  check(!shaper.surging(later), "surge expires after the hold window");
  const auto rates = shaper.rates_at(later, kFloor);
  check(rates.full_hz == kFloor.full_hz, "floor returns after the surge");
}

void test_rotate_counts_small_jitter_does_not() {
  const auto now = Clock::now();
  // Prime one frame first: the very first sample is a round transition and
  // always surges, which would mask the movement signal under test.
  const auto primed = now - std::chrono::seconds(10);

  BehaviorShaper jitter;
  jitter.note_round(quiet_round(), 0.f, primed);
  jitter.note_round(quiet_round(), 40.f, now);
  check(!jitter.surging(now), "small movement does not surge");

  BehaviorShaper rotate;
  rotate.note_round(quiet_round(), 0.f, primed);
  rotate.note_round(quiet_round(), 400.f, now);
  check(rotate.surging(now), "real reposition surges");
}

void test_no_repeat_surge_without_change() {
  BehaviorShaper shaper;
  const auto now = Clock::now();
  auto planted = bomb_planted();
  shaper.note_round(planted, 0.f, now);
  const auto after = now + std::chrono::seconds(10);
  shaper.note_round(planted, 0.f, after);
  check(!shaper.surging(after), "holding the same state does not re-surge");
}

// ── Ladder ───────────────────────────────────────────────────────
void test_ladder_steps_down_and_holds() {
  DegradeLadder ladder;
  ReadRates out{};
  ladder.step(80, kFloor, out);
  check(ladder.level == 1, "a hot window steps the level down");
  ladder.step(80, kFloor, out);
  check(ladder.level == 2, "a second hot window steps again");
  check(out.far_hz <= 2, "level 2 throttles far scans");
}

void test_ladder_floor_disables_far() {
  DegradeLadder ladder;
  ReadRates out{};
  for (int i = 0; i < 5; ++i) ladder.step(90, kFloor, out);
  check(ladder.level == DegradeLadder::kMaxLevel, "level saturates at the floor");
  check(out.far_hz == 0, "the floor turns far scans off");
}

void test_relax_needs_three_clean_windows() {
  DegradeLadder ladder;
  ReadRates out{};
  ladder.step(90, kFloor, out);
  const int start = ladder.level;
  ladder.step(5, kFloor, out);
  check(ladder.level == start, "one clean window does not relax");
  ladder.step(5, kFloor, out);
  check(ladder.level == start, "two clean windows do not relax");
  ladder.step(5, kFloor, out);
  check(ladder.level == start - 1, "three clean windows relax exactly one rung");
}

// ── Composition ──────────────────────────────────────────────────
void test_ladder_survives_the_shaper() {
  // This is the regression the split header exists for: applying the shaper
  // after the ladder would restore full speeds and lose the degrade.
  BehaviorShaper shaper;
  DegradeLadder ladder;
  ReadRates out{};
  const auto now = Clock::now();
  for (int i = 0; i < 4; ++i) ladder.step(90, kFloor, out);

  const auto posture = apply_posture(shaper, ladder, IntelPolicy::Auto, now, kFloor);
  check(posture.rates.far_hz == 0, "degraded far-off survives event shaping");
  check(posture.rates.full_hz <= 6, "degraded full rate survives event shaping");

  // Even mid-surge the cap holds.
  shaper.note_round(bomb_planted(), 0.f, now);
  const auto surging = apply_posture(shaper, ladder, IntelPolicy::Auto, now, kFloor);
  check(surging.rates.full_hz <= 6, "surge cannot exceed the degraded cap");
}

void test_auto_never_relaxes_past_hud_echo() {
  BehaviorShaper shaper;
  DegradeLadder ladder;
  ReadRates out{};
  const auto now = Clock::now();
  const auto clean = apply_posture(shaper, ladder, IntelPolicy::Auto, now, kFloor);
  check(clean.mode == IntelMode::Full, "auto starts at full intel");

  ladder.step(90, kFloor, out);
  const auto degraded = apply_posture(shaper, ladder, IntelPolicy::Auto, now, kFloor);
  check(degraded.mode == IntelMode::HudEcho, "risk falls back to hud-echo");
}

void test_pinned_policy_wins() {
  BehaviorShaper shaper;
  DegradeLadder ladder;
  ReadRates out{};
  const auto now = Clock::now();
  for (int i = 0; i < 4; ++i) ladder.step(90, kFloor, out);

  const auto pinned_full =
      apply_posture(shaper, ladder, IntelPolicy::Full, now, kFloor);
  check(pinned_full.mode == IntelMode::Full, "pinned full resists the ladder");
  check(pinned_full.rates.far_hz == 0, "pinned intel still respects the rates");

  const auto pinned_echo =
      apply_posture(shaper, ladder, IntelPolicy::HudEcho, now, kFloor);
  check(pinned_echo.mode == IntelMode::HudEcho, "pinned hud-echo sticks");
}

void test_policy_cycle_order() {
  check(next_intel_policy(IntelPolicy::Auto) == IntelPolicy::HudEcho,
        "auto cycles to pinned hud-echo");
  check(next_intel_policy(IntelPolicy::HudEcho) == IntelPolicy::Full,
        "pinned hud-echo cycles to pinned full");
  check(next_intel_policy(IntelPolicy::Full) == IntelPolicy::Auto,
        "pinned full cycles back to auto");
}

}  // namespace

int main() {
  test_quiet_floor();
  test_event_burst_is_bounded();
  test_burst_expires();
  test_rotate_counts_small_jitter_does_not();
  test_no_repeat_surge_without_change();
  test_ladder_steps_down_and_holds();
  test_ladder_floor_disables_far();
  test_relax_needs_three_clean_windows();
  test_ladder_survives_the_shaper();
  test_auto_never_relaxes_past_hud_echo();
  test_pinned_policy_wins();
  test_policy_cycle_order();

  if (failures != 0) {
    std::printf("%d posture check(s) failed\n", failures);
    return 1;
  }
  std::printf("stack_posture_test: all checks passed\n");
  return 0;
}
