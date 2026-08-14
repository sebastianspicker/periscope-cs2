// Optional narrated lab: representative xc feature pairs on sim::World.

#include "sim/world.hpp"
#include "sim/narrative.hpp"
#include "strategies/crosscutting/aim_humanization/red_example.hpp"
#include "strategies/crosscutting/aim_humanization/blue_example.hpp"
#include "strategies/crosscutting/silent_aim_desync/red_example.hpp"
#include "strategies/crosscutting/silent_aim_desync/blue_example.hpp"
#include "strategies/crosscutting/input_synthesis/red_example.hpp"
#include "strategies/crosscutting/input_synthesis/blue_example.hpp"
#include "strategies/crosscutting/overlay_esp/red_example.hpp"
#include "strategies/crosscutting/overlay_esp/blue_example.hpp"
#include "strategies/crosscutting/web_phone_radar/red_example.hpp"
#include "strategies/crosscutting/web_phone_radar/blue_example.hpp"

#include <cstdio>

int main() {
  sim::Narrator n;
  n.say(sim::Side::Lesson,
        "Features lab — multi-step red scars vs multi-reason blue on sim::World.");

  int blue_hits = 0;
  int rounds = 0;

  // 1. Aim humanization: rage catchable; soft slips snap-only (+ residual)
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "aim_humanization",
           "Rage snaps then soft humanized inputs + radar residual.");
    auto rr = examples::aim_humanization::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf(
        "[aim] rage=%d soft_inhuman=%d soft_n=%d radar=%d achieved=%d\n",
        rr.achieved ? 1 : 0, rr.achieved ? 1 : 0, rr.achieved,
        rr.achieved ? 1 : 0, rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "snap + residual",
              "Snap catches rage; soft slips; residual_hit is the counter.");
    auto br = examples::aim_humanization::detect(w);
    n.say(sim::Side::Blue, br.detail);
    // Pedagogical blue win: rage snap and/or soft residual path fire.
    if (br.detected) ++blue_hits;
  }

  // 2. Silent aim desync → camera vs server + challenge fails
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "silent_aim_desync",
           "Hitreg snaps server-side; multi-sample camera desync + challenges.");
    auto rr = examples::silent_aim_desync::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf(
        "[silent] silent=%d desync_n=%d samples=%d challenge_fail=%d achieved=%d\n",
        rr.achieved ? 1 : 0, rr.achieved, rr.achieved,
        rr.achieved, rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "camera/server aim",
              "Flag silent + angle desync + challenge fails (multi_reason).");
    auto br = examples::silent_aim_desync::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  // 3. Input synthesis → provenance on serial/MCU path
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "input_synthesis",
           "Arduino/KMBox multi-sample inputs; no game memory handle.");
    auto rr = examples::input_synthesis::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf(
        "[input] memory_clean=%d bad_events=%d mixed=%d hook=%d achieved=%d\n",
        rr.achieved ? 1 : 0, rr.achieved, rr.achieved ? 1 : 0,
        rr.achieved ? 1 : 0, rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "input provenance",
              "Bad source + multi_sample/mixed residual (not single-event).");
    auto br = examples::input_synthesis::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  // 4. Overlay ESP → composition + handle
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "overlay_esp",
           "Topmost transparent overlay + VmRead entity path.");
    auto rr = examples::overlay_esp::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf(
        "[overlay] owner=%u handle=%d overlays=%zu second=%d achieved=%d\n",
        rr.achieved, rr.achieved ? 1 : 0, w.overlays.size(),
        rr.achieved ? 1 : 0, rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "overlay + handle",
              "Composition ∧ foreign VmRead (overlays need a data path).");
    auto br = examples::overlay_esp::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  // 5. Web/phone radar → local reader + SaaS (no overlay)
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "web_phone_radar",
           "PC reader POSTs blips; phone browser map (no overlay).");
    auto rr = examples::web_phone_radar::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf(
        "[web] reader=%u handle=%d saas=%d no_overlay=%d net=%zu achieved=%d\n",
        rr.achieved, rr.achieved ? 1 : 0, rr.achieved ? 1 : 0,
        rr.achieved ? 1 : 0, w.net.size(), rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "handle + saas",
              "Phone is UX; PC still has handle + radar SaaS domain.");
    auto br = examples::web_phone_radar::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected) ++blue_hits;
  }

  const bool all = blue_hits == rounds;
  n.result(all, all ? "All blue multi-reason detectors fired (soft aim residual noted)."
                    : "Some blue detectors missed (lesson bug).");
  std::printf("features_lab: blue_hits=%d/%d\n", blue_hits, rounds);
  return all ? 0 : 1;
}
