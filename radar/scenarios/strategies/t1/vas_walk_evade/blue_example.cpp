#include "blue_example.hpp"
#include <cstdio>

namespace examples::vas_walk_evade {

BlueResult detect(sim::World& w) {
    BlueResult result{};
    const auto game_pid = w.game_pid();
    const auto* game = w.proc(game_pid);
    if (!game || !game->is_game) {
        result.reasons.emplace_back("sensor precondition failed: game unavailable");
        result.signals = static_cast<int>(result.reasons.size());
        return result;
    }

    // Check for VEH handler anomalies (lab baseline is typically 0–2).
    if (w.veh_handlers > 2 || !w.veh_chain_clean) {
        result.reasons.emplace_back("unexpected VEH handler count=" +
            std::to_string(w.veh_handlers) +
            (w.veh_chain_clean ? "" : " (chain dirty)"));
    }

    // Check for modules with erased headers — strong VAS-walk evasion scar.
    int erasedHeaders = 0;
    for (const auto& mod : game->modules) {
        if (mod.headers_erased) erasedHeaders++;
    }
    if (erasedHeaders > 0) {
        result.reasons.emplace_back("module header erasure detected count=" +
            std::to_string(erasedHeaders));
    }

    result.signals = static_cast<int>(result.reasons.size());
    // Multi-reason: require VEH residual and header-erasure residual (or >=2 reasons).
    result.detected = result.signals >= 2;
    result.risk = result.detected ? 0.7 + 0.1 * result.signals : 0.0;

    // Mitigate: strip extra VEH handlers and restore module headers.
    if (result.detected) {
        w.veh_handlers = 0;
        w.veh_chain_clean = true;
        for (auto& mod : w.proc(game_pid)->modules) {
            if (mod.headers_erased) {
                mod.headers_erased = false;
                if (mod.base == 0) mod.base = 0x140000000ULL;
            }
        }
        result.mitigated = true;
        result.reasons.emplace_back("mitigated: VEH handlers cleared and headers restored");
        result.signals = static_cast<int>(result.reasons.size());
    }

    std::printf("[T1 vas_walk_evade] BLUE signals=%d detected=%s mitigated=%s\n",
        result.signals, result.detected ? "true" : "false",
        result.mitigated ? "true" : "false");
    for (const auto& r : result.reasons) {
        std::printf("[T1 vas_walk_evade]   %s\n", r.c_str());
    }
    return result;
}

}
