// Optional narrated lab: a few representative xc evasion pairs on sim::World.
// Educational demo — multi-step red scars vs multi-reason blue.

#include "sim/world.hpp"
#include "sim/narrative.hpp"
#include "strategies/crosscutting/anti_re_canary/red_example.hpp"
#include "strategies/crosscutting/anti_re_canary/blue_example.hpp"
#include "strategies/crosscutting/hwid_spoof/red_example.hpp"
#include "strategies/crosscutting/hwid_spoof/blue_example.hpp"
#include "strategies/crosscutting/staged_loader/red_example.hpp"
#include "strategies/crosscutting/staged_loader/blue_example.hpp"
#include "strategies/crosscutting/clipboard_token/red_example.hpp"
#include "strategies/crosscutting/clipboard_token/blue_example.hpp"

#include <cstdio>

int main() {
  sim::Narrator n;
  n.say(sim::Side::Lesson,
        "Evasion lab — multi-step red scars vs multi-reason blue on sim::World.");

  int blue_hits = 0;
  int rounds = 0;

  // 1. HWID spoof → payment/IP cluster (generic residual template)
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "hwid_spoof",
           "Rotate HWID, plant smurf + banned accounts sharing payment/IP.");
    auto rr = examples::hwid_spoof::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[hwid] now=%s accounts=%zu achieved=%d actor=%u\n",
                w.trust.hwid.c_str(), w.accounts.size(), rr.achieved ? 1 : 0,
                rr.actor_pid);
    n.counter(sim::Side::Blue, "payment/ip cluster",
              "HWID alone is soft; graph on payment_fp + ip_class.");
    auto br = examples::hwid_spoof::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected || br.mitigated) ++blue_hits;
  }

  // 2. Staged loader → runtime residual + C2 surface
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "staged_loader",
           "Tiny stub, manual-map RX payload, CDN fetch.");
    auto rr = examples::staged_loader::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[staged] actor=%u net=%zu achieved=%d\n", rr.actor_pid,
                w.net.size(), rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "runtime + net",
              "Disk YARA blind; private RX + C2 hit.");
    auto br = examples::staged_loader::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected || br.mitigated) ++blue_hits;
  }

  // 3. Clipboard token leak
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "clipboard_token",
           "Session/auth material left on clipboard.");
    auto rr = examples::clipboard_token::apply(w);
    n.say(sim::Side::Red, rr.detail);
    n.counter(sim::Side::Blue, "clipboard sensor",
              "Flag clipboard_token_leak scar.");
    auto br = examples::clipboard_token::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected || br.mitigated) ++blue_hits;
  }

  // 4. Anti-RE canary
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "anti_re_canary",
           "Mark analysis host and trip canary (refuse/decoy path).");
    auto rr = examples::anti_re_canary::apply(w);
    n.say(sim::Side::Red, rr.detail);
    n.counter(sim::Side::Blue, "analyst env",
              "analysis_host || canary_tripped.");
    auto br = examples::anti_re_canary::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected || br.mitigated) ++blue_hits;
  }

  const bool all = blue_hits == rounds;
  n.result(all, all ? "All blue multi-reason detectors fired."
                    : "Some blue detectors missed (lesson bug).");
  std::printf("evasion_lab: blue_hits=%d/%d\n", blue_hits, rounds);
  return all ? 0 : 1;
}
