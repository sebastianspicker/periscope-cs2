#include "red_example.hpp"
#include <cstdio>

namespace examples::norm_hash_evade {

RedResult apply(sim::World& w) {
    int steps = 0;
    const auto game_pid = w.game_pid();
    auto* game = w.proc(game_pid);
    if (!game || !game->is_game) {
        std::printf("[T1 norm_hash_evade] FAIL: game process unavailable\n");
        return {false, steps, "precondition failed: game process unavailable"};
    }
    std::printf("[T1 norm_hash_evade] step %d: game pid=%u validated\n", ++steps, game_pid);

    const auto actor = w.spawn("norm_hash_evade-actor.exe");
    auto* actor_process = w.proc(actor);
    if (!actor_process) {
        return {false, steps, "actor spawn verification failed"};
    }
    std::printf("[T1 norm_hash_evade] step %d: actor pid=%u created\n", ++steps, actor);

    // Hook PEFile::NormalizedHash by replacing the module's text_hash.
    // Re-resolve game after spawn (map rehash may invalidate Process*).
    game = w.proc(game_pid);
    if (!game) {
        return {false, steps, "game process lost after actor spawn"};
    }
    for (auto& mod : game->modules) {
        if (mod.name.find("game.exe") != std::string::npos ||
            mod.name.find("client.dll") != std::string::npos) {
            mod.text_hash = "clean";
            mod.iat_hooked = true;
        }
    }
    w.normalized_hash_evade_active = true;
    w.pe_hash_normalized = true;
    w.pe_section_entries_altered =
        static_cast<int>(game->modules.size() >= 2 ? 2 : 1);
    std::printf("[T1 norm_hash_evade] step %d: module hashes cleaned\n", ++steps);

    w.note("norm_hash_evade: PEFile::NormalizedHash patched to return cached clean hash");
    return {true, steps, "norm_hash_evade: module hashes replaced with clean cached values"};
}

}
