// RED example implementation for this strategy pair.
// Multi-step World scars for learners; pairs with blue_example detect/mitigate.

#include "red_example.hpp"

#include <cstdio>

namespace examples::mapper_artifact {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game = w.game_pid();
  std::printf("[red:mapper_artifact] verify a game is present\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, steps, "precondition failed: game process missing"};
  }
  ++steps;

  std::printf("[red:mapper_artifact] spawn kdmapper-style mapper process\n");
  const auto actor = w.spawn("kdmapper.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, steps, "mapper process spawn failed"};
  }
  ++steps;

  std::printf(
      "[red:mapper_artifact] mark mapper_process_present + pool_tag_anomaly\n");
  w.mapper_process_present = true;
  w.pool_tag_anomaly = true;
  if (!w.mapper_process_present) {
    return {false, steps, "mapper_process_present scar failed"};
  }
  ++steps;

  std::printf(
      "[red:mapper_artifact] load unsigned memory-capable mapped driver\n");
  sim::Driver mapped;
  mapped.name = "mapped.sys";
  mapped.sha256 = "unsigned_mapper_payload";
  mapped.signer = "unsigned";
  mapped.boot_start = false;
  mapped.byovd_known_bad = false;
  mapped.is_ac = false;
  mapped.is_bridge = false;
  mapped.provides_mem_rw = true;
  mapped.load_order = 90;
  w.load_driver(mapped);
  ++steps;

  bool unsigned_path_loaded = false;
  for (const auto& d : w.drivers) {
    if (d.name == "mapped.sys" && d.provides_mem_rw && !d.is_ac) {
      unsigned_path_loaded = true;
      break;
    }
  }
  if (!unsigned_path_loaded) {
    return {false, steps, "unsigned mapper driver not loaded"};
  }
  ++steps;

  const bool achieved =
      w.mapper_process_present && actor != 0 && unsigned_path_loaded;
  RedResult r{achieved, steps,
              "kdmapper process + pool tag + unsigned mem_rw driver loaded"};
  w.note(r.detail);
  std::printf("[red:mapper_artifact] %s (actor=%u pool=%d)\n", r.detail.c_str(),
              actor, static_cast<int>(w.pool_tag_anomaly));
  return r;
}

}  // namespace examples::mapper_artifact
