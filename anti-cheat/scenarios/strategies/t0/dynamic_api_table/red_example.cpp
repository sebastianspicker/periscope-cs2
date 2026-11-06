#include "red_example.hpp"
#include <cstdio>

namespace examples::dynamic_api_table {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game)
    return {false, steps, "game unavailable"};

  // Phase 1: spawn resolver actor that will walk PEB/EAT.
  const auto resolver = w.spawn("api-resolver.exe");
  if (!w.proc(resolver))
    return {false, steps, "resolver spawn failed"};
  std::printf("[T0 dynamic_api_table] step %d: resolver actor spawned\n", ++steps);

  // Phase 2: plant dynamic import resolution scar (PEB+EAT walk narrative).
  w.dynamic_import_resolution = true;
  w.dynamic_import_count = 24;
  if (!w.dynamic_import_resolution || w.dynamic_import_count < 20)
    return {false, steps, "dynamic API table scar failed"};
  std::printf("[T0 dynamic_api_table] step %d: PEB+EAT walked, %d APIs resolved\n",
              ++steps, w.dynamic_import_count);

  // Phase 3: IAT residual on a game module (second independent scar class).
  if (!game->modules.empty()) {
    game->modules[0].iat_hooked = true;
  } else {
    sim::Module m;
    m.name = "ntdll.dll";
    m.base = game->base + 0x10000;
    m.size = 0x2000;
    m.iat_hooked = true;
    game->modules.push_back(m);
  }
  std::printf("[T0 dynamic_api_table] step %d: IAT residual planted on module surface\n",
              ++steps);

  const bool scar = w.dynamic_import_resolution && w.dynamic_import_count >= 20;
  if (!scar)
    return {false, steps, "dynamic API multi-scar verification failed"};

  w.note("dynamic_api_table: PEB+EAT dynamic API resolution + IAT residual");
  return {true, steps, "dynamic_api_table: PEB+EAT resolution", w.dynamic_import_count, true,
          true};
}

}  // namespace examples::dynamic_api_table
