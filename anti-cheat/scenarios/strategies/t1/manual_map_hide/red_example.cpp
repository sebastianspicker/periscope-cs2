// RED example implementation for this strategy pair.
// Multi-step World scars for learners; pairs with blue_example detect/mitigate.

#include "red_example.hpp"

#include <cstdio>

namespace examples::manual_map_hide {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game = w.game_pid();
  std::printf("[red:manual_map_hide] verify a game is present\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, steps, "precondition failed: game process missing"};
  }
  ++steps;

  std::printf("[red:manual_map_hide] spawn external actor (loader facade)\n");
  const auto actor = w.spawn("manual-map-loader.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, steps, "actor creation failed"};
  }
  ++steps;

  std::printf(
      "[red:manual_map_hide] manual-map inject: unlinked PE, erased headers\n");
  sim::Module payload;
  payload.name = "lab-map.dll";
  payload.base = 0x70000000ull;
  payload.size = 4096;
  payload.linked_in_peb = false;
  payload.headers_erased = true;
  payload.text_hash = "foreign";
  if (!w.inject_module(game, payload, /*manual_map=*/true)) {
    return {false, steps, "manual-map inject failed"};
  }
  ++steps;

  auto* g = w.proc(game);
  if (g == nullptr) {
    return {false, steps, "game process disappeared"};
  }
  // inject_module(manual_map) already sets these; re-assert for the lesson.
  g->has_foreign_thread = true;
  g->manual_mapped_region = true;
  ++steps;

  bool present = false;
  bool peb_linked = true;
  for (const auto& m : g->modules) {
    if (m.name == "lab-map.dll") {
      present = true;
      peb_linked = m.linked_in_peb;
      break;
    }
  }
  if (!present) {
    return {false, steps, "injected module not present in game module list"};
  }
  ++steps;

  if (peb_linked) {
    return {false, steps, "module still PEB-linked (manual map failed)"};
  }
  if (!g->manual_mapped_region) {
    return {false, steps, "manual_mapped_region scar missing"};
  }
  if (!g->has_foreign_thread) {
    return {false, steps, "foreign thread scar missing"};
  }
  ++steps;

  const bool achieved =
      present && !peb_linked && g->manual_mapped_region && g->has_foreign_thread;
  RedResult r{achieved, steps,
              "manual-mapped module present, PEB-unlinked, region+thread scars"};
  w.note(r.detail);
  std::printf("[red:manual_map_hide] %s\n", r.detail.c_str());
  return r;
}

}  // namespace examples::manual_map_hide
