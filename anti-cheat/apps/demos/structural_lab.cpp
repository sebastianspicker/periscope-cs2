// Optional narrated lab: structural xc pairs on sim::World.
// Educational demo — exercises apply/detect and prints multi-step residuals.

#include "sim/world.hpp"
#include "sim/narrative.hpp"
#include "strategies/crosscutting/delayed_ban/red_example.hpp"
#include "strategies/crosscutting/delayed_ban/blue_example.hpp"
#include "strategies/crosscutting/interest_mgmt/red_example.hpp"
#include "strategies/crosscutting/interest_mgmt/blue_example.hpp"
#include "strategies/crosscutting/info_advantage/red_example.hpp"
#include "strategies/crosscutting/info_advantage/blue_example.hpp"
#include "strategies/crosscutting/fallback_chain/red_example.hpp"
#include "strategies/crosscutting/fallback_chain/blue_example.hpp"
#include "strategies/crosscutting/entity_stream_crypto/red_example.hpp"
#include "strategies/crosscutting/entity_stream_crypto/blue_example.hpp"

#include <cstdio>

int main() {
  sim::Narrator n;
  n.say(sim::Side::Lesson,
        "Structural lab — multi-step red scars vs blue detect/mitigate on "
        "sim::World.");

  int blue_hits = 0;
  int rounds = 0;

  // 1. Delayed / correlated bans
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "delayed_ban",
           "Private build / short life — stay under instant thresholds.");
    auto rr = examples::delayed_ban::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[delayed_ban] achieved=%d log=%zu actor=%u\n",
                rr.achieved ? 1 : 0, w.log.size(), rr.actor_pid);
    n.counter(sim::Side::Blue, "BanCorrelator",
              "Multi-signal delayed confidence — kill 'UD forever' marketing.");
    auto br = examples::delayed_ban::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected || br.mitigated) ++blue_hits;
  }

  // 2. Interest management / fog-of-war
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "interest_mgmt",
           "Any memory radar wants full enemy XY in client memory.");
    auto rr = examples::interest_mgmt::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[interest] full_repl=%d achieved=%d actor=%u\n",
                w.server_sends_full_enemy_origin ? 1 : 0, rr.achieved ? 1 : 0,
                rr.actor_pid);
    n.counter(sim::Side::Blue, "fog-of-war",
              "Do not replicate unobservable enemies at full fidelity.");
    auto br = examples::interest_mgmt::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected || br.mitigated) ++blue_hits;
  }

  // 3. Info-advantage behavior
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "info_advantage",
           "Legit radar play: human aim with unfair knowledge (no aimbot).");
    auto rr = examples::info_advantage::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[info] achieved=%d actor=%u\n", rr.achieved ? 1 : 0,
                rr.actor_pid);
    n.counter(sim::Side::Blue, "server demo features",
              "Align aim with vision/audio truth; multi-invariant score.");
    auto br = examples::info_advantage::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected || br.mitigated) ++blue_hits;
  }

  // 4. Tier fallback chain
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "fallback_chain",
           "Try HV; else kernel; else RPM. Reliability over purity.");
    auto rr = examples::fallback_chain::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf("[fallback] achieved=%d actor=%u\n", rr.achieved ? 1 : 0,
                rr.actor_pid);
    n.counter(sim::Side::Blue, "full detector stack",
              "Hunting only T3 misses the RPM fallback — stack sensors.");
    auto br = examples::fallback_chain::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected || br.mitigated) ++blue_hits;
  }

  // 5. Entity stream crypto residual
  {
    ++rounds;
    auto w = sim::make_arena();
    n.move(sim::Side::Red, "entity_stream_crypto",
           "Encrypted stream still free XY if client holds the key.");
    auto rr = examples::entity_stream_crypto::apply(w);
    n.say(sim::Side::Red, rr.detail);
    std::printf(
        "[stream] enc=%d key=%d exfil=%d achieved=%d\n",
        w.entity_stream_encrypted ? 1 : 0, w.client_has_stream_key ? 1 : 0,
        w.stream_key_exfiltrated ? 1 : 0, rr.achieved ? 1 : 0);
    n.counter(sim::Side::Blue, "no client key + fog",
              "Encrypt without client key; pair with interest management.");
    auto br = examples::entity_stream_crypto::detect(w);
    n.say(sim::Side::Blue, br.detail);
    if (br.detected || br.mitigated) ++blue_hits;
  }

  const bool all = blue_hits == rounds;
  n.result(all, all ? "All blue detect/mitigate paths won (structural lessons)."
                    : "Some blue paths missed (lesson bug).");
  std::printf("structural_lab: blue_hits=%d/%d\n", blue_hits, rounds);
  return all ? 0 : 1;
}
