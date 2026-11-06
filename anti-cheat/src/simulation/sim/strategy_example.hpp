#pragma once

#include "sim/world.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace sim::strategy_example {

struct RedOutcome {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

struct BlueOutcome {
  bool detected = false;
  int signals = 0;
  std::vector<std::string> reasons;
  double risk = 0.0;
};

inline RedOutcome fail_red(std::string strategy, int steps, std::string reason) {
  std::printf("[red:%s] failed: %s\n", strategy.c_str(), reason.c_str());
  return {false, steps, std::move(reason)};
}

inline RedOutcome record_red(sim::World& w, const char* strategy,
                             const char* actor_name,
                             const std::vector<std::pair<std::string, bool>>& steps) {
  RedOutcome result;
  if (w.game_pid() == 0 || w.proc(w.game_pid()) == nullptr) {
    return fail_red(strategy, result.steps, "precondition failed: game process is unavailable");
  }

  const auto actor = w.spawn(actor_name);
  if (actor == 0 || w.proc(actor) == nullptr) {
    return fail_red(strategy, result.steps, "precondition failed: lab actor could not start");
  }

  for (const auto& [description, succeeded] : steps) {
    std::printf("[red:%s] step %d: %s\n", strategy, result.steps + 1,
                description.c_str());
    if (!succeeded) {
      return fail_red(strategy, result.steps, "step failed: " + description);
    }
    ++result.steps;
  }

  result.achieved = true;
  result.detail = std::string(strategy) + " completed " +
                  std::to_string(result.steps) + " lab-only steps";
  w.note(result.detail);
  return result;
}

inline void add_signal(BlueOutcome& result, std::string reason, double weight) {
  ++result.signals;
  result.risk += weight;
  result.reasons.push_back(std::move(reason));
}

inline BlueOutcome finish_blue(const char* strategy, BlueOutcome result) {
  result.risk = std::min(1.0, result.risk);
  // Multi-reason bar: require at least two independent signal reasons.
  result.detected = result.signals >= 2;
  std::printf("[blue:%s] signals=%d risk=%.2f\n", strategy, result.signals,
              result.risk);
  return result;
}

inline void add_common_signals(sim::World& w, BlueOutcome& result) {
  const auto game = w.game_pid();
  if (game == 0 || w.proc(game) == nullptr) {
    add_signal(result, "game process missing during inspection", 0.35);
    return;
  }
  for (const auto& handle : w.handles_to(game, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      add_signal(result, "untrusted process holds game memory-read access", 0.30);
      break;
    }
  }
  if (const auto* process = w.proc(game);
      process != nullptr && (process->has_foreign_thread || process->manual_mapped_region ||
                             process->thread_hijacked || process->hollowed)) {
    add_signal(result, "game process has an unexpected execution artifact", 0.25);
  }
  if (std::any_of(w.drivers.begin(), w.drivers.end(), [](const auto& driver) {
        return !driver.is_ac && (driver.provides_mem_rw || driver.byovd_known_bad);
      })) {
    add_signal(result, "memory-capable or known-bad driver is present", 0.35);
  }
}

}  // namespace sim::strategy_example
