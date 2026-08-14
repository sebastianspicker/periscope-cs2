#include "blue_example.hpp"
#include <cstdio>

namespace examples::norm_hash_evade {

BlueResult detect(sim::World& w) {
    BlueResult result{};
    const auto game_pid = w.game_pid();
    const auto* game = w.proc(game_pid);
    if (!game || !game->is_game) {
        result.reasons.emplace_back("sensor precondition failed: game unavailable");
        result.signals = static_cast<int>(result.reasons.size());
        return result;
    }

    // Check for IAT hooks on game modules
    int hookedModules = 0;
    int cleanModules = 0;
    for (const auto& mod : game->modules) {
        if (mod.iat_hooked) hookedModules++;
        if (mod.text_hash == "clean") cleanModules++;
    }

    if (hookedModules > 0) {
        result.reasons.emplace_back("IAT hooks detected count=" +
            std::to_string(hookedModules));
    }

    if (cleanModules >= 1 && hookedModules > 0) {
        result.reasons.emplace_back("clean text_hash coexists with IAT hooks count=" +
            std::to_string(cleanModules));
    }

    if (w.normalized_hash_evade_active || w.pe_hash_normalized) {
        result.reasons.emplace_back("normalized PE hash evade residual active");
    }

    if (w.pe_section_entries_altered > 0) {
        result.reasons.emplace_back("PE section entries altered count=" +
            std::to_string(w.pe_section_entries_altered));
    }

    result.signals = static_cast<int>(result.reasons.size());
    // Multi-reason: hooks plus independent clean-hash / normalized-hash residual.
    result.detected = result.signals >= 2 && hookedModules > 0;
    if (result.detected) {
        result.mitigated = true;
    }

    std::printf("[T1 norm_hash_evade] BLUE signals=%d detected=%s\n",
        result.signals, result.detected ? "true" : "false");
    for (const auto& r : result.reasons) {
        std::printf("[T1 norm_hash_evade]   %s\n", r.c_str());
    }
    return result;
}

}
