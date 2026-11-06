#include "red_example.hpp"
#include <cstdio>
#include <string>

namespace examples::vas_walk_evade {

RedResult apply(sim::World& w) {
    int steps = 0;
    const auto game_pid = w.game_pid();
    auto* game = w.proc(game_pid);
    if (!game || !game->is_game) {
        std::printf("[T1 vas_walk_evade] FAIL: game process unavailable\n");
        return {false, steps, "precondition failed: game process unavailable"};
    }
    std::printf("[T1 vas_walk_evade] step %d: game pid=%u validated\n", ++steps, game_pid);

    const auto actor = w.spawn("vas_walk_evade-actor.exe");
    auto* actor_process = w.proc(actor);
    if (!actor_process) {
        return {false, steps, "actor spawn verification failed"};
    }
    std::printf("[T1 vas_walk_evade] step %d: actor pid=%u created\n", ++steps, actor);

    // Educational scar: extra VEH handlers used to hide code pages during VAS walk.
    // Blue thresholds treat handler counts above the lab baseline as anomalous.
    if (w.veh_handlers < 3) {
        w.veh_handlers = 3;
    } else {
        ++w.veh_handlers;
    }
    w.veh_chain_clean = false;

    // Register VEH handler to temporarily hide code pages during VAS walk
    bool erased = false;
    for (auto& mod : game->modules) {
        if (mod.name.find("game") != std::string::npos || mod.name.find(".exe") != std::string::npos) {
            mod.base = 0;
            mod.headers_erased = true;
            erased = true;
        }
    }
    if (!erased && !game->modules.empty()) {
        game->modules.front().headers_erased = true;
        game->modules.front().base = 0;
        erased = true;
    }
    if (!erased) {
        // Ensure a module scar exists even if plant data uses unusual names.
        sim::Module scar;
        scar.name = "game.exe";
        scar.base = 0;
        scar.size = 0x1000;
        scar.headers_erased = true;
        game->modules.push_back(std::move(scar));
    }
    std::printf("[T1 vas_walk_evade] step %d: VEH handler registered (veh=%zu headers_erased)\n",
                ++steps, w.veh_handlers);

    w.note("vas_walk_evade: code pages hidden via VEH during VAS walk phase");
    return {true, steps, "vas_walk_evade: VEH handler hides code pages during VAS walk"};
}

}
