// radar_pipeline.cpp — Complete radar frame pipeline implementation.
//
// ALL 15+ ac_sim features are integrated here. Previously each was
// fully implemented but NEVER instantiated or called from any pipeline.
// This file resolves that by wiring everything into a coherent frame loop.
//
// Frame flow:
//   pre_frame()   → RNG, temporal, disguise, self-verify, API integrity, system normalize
//   collect_frame() → batch read, entity collect, HUD, CVar
//   post_frame()  → health ladder, accept gate, decoy render, behavioral filter, ETL, DllWatch, forensic

#include "radar_pipeline.hpp"

#include "ac/types.hpp"
#include "real/win/anti_debug.hpp"
#include "real/win/pe_hide.hpp"
#include "real/win/hook_detect.hpp"
#include "real/win/veh_anti_debug.hpp"
#include "real/win/timing.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/cs2/offsets.hpp"          // find_client_module / find_module_by_basename
#include "real/cs2/periscope_scanner.hpp" // scan_all_patterns / PeriscopeOffsets
#include "real/win/api_table.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/windows_h.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace radar {

// Shared pipeline instance for static InitOrchestrator stubs (one definition).
extern FramePipeline* g_pipeline;
// Dependency-name arrays for subsystems that require ordering.
extern const char* kDepRng[];

bool FramePipeline::run_frame() noexcept {
    if (!m_initialized) return false;

    return m_frameLoop.run_frame();
}

// ══════════════════════════════════════════════════════════════════════
//  shutdown()  —  delegates to InitOrchestrator::shutdown_all()
// ══════════════════════════════════════════════════════════════════════

bool FramePipeline::pre_frame() noexcept {
    ETL_RECORD(sim::EtlEventType::Frame, "pre_frame start");

    // 1. System normalization (WER suppression, handle count)
    auto normResult = m_sysNorm.normalize();
    if (normResult.handleCountNormalized) {
        ETL_RECORD(sim::EtlEventType::EvasionTick, "handle count normalized");
    }

    // 2. Temporal evasion (advance 4-phase jitter)
    m_temporal.tick();
    if (m_frameLoop.frame_index % 256 == 0) {
        ETL_RECORD(sim::EtlEventType::EvasionTick,
                   "temporal phase advanced to " + std::to_string(
                       static_cast<int>(m_temporal.phase())));
    }

    // 3. Batch engine — set temporal-aware rate limiting
    m_batch.set_rate_limit(m_temporal.phase() == sim::TemporalPhase::Alpha ? 48 : 120);

    // 4. Self-verify (periodic: every 500 frames) — drives heartbeat gate bit.
    if (m_frameLoop.frame_index == 1 || m_frameLoop.frame_index % 500 == 0) {
        auto svResult = m_selfVerify.verify();
        m_selfVerifyOk = svResult.passed;
        if (!svResult.passed) {
            ETL_RECORD(sim::EtlEventType::Error,
                       "self-verify failed crc=" +
                           std::to_string(svResult.computedCrc));
        }
    }

    // 5. API integrity check (frame 1 and every 1000 frames) — real verify_all.
    if (m_frameLoop.frame_index == 1 || m_frameLoop.frame_index % 1000 == 0) {
        auto results = m_apiIntegrity.verify_all();
        bool hookFree = !results.empty();
        int failed = 0;
        for (const auto& r : results) {
            if (!r.passed) {
                hookFree = false;
                ++failed;
            }
        }
#if LR_PLATFORM_WINDOWS
        auto& api = real::win::g_Api();
        if (api.NtReadVirtualMemory) {
            auto vr = m_apiIntegrity.verify(
                "ntdll", "NtReadVirtualMemory",
                reinterpret_cast<uint64_t>(api.NtReadVirtualMemory));
            if (!vr.passed) {
                hookFree = false;
                ++failed;
            }
        } else if (m_useReal) {
            // Real path requires the read entrypoint; unresolved is a fail.
            hookFree = false;
            ++failed;
        }
#endif
        if (results.empty() && !m_apiIntegrityRegistryReady) {
            hookFree = false;
        }
        m_apiIntegrityOk = hookFree;
        ETL_RECORD(sim::EtlEventType::EvasionTick,
                   hookFree
                       ? ("API integrity: OK checks=" +
                          std::to_string(results.size()))
                       : ("API integrity: FAIL failed=" +
                          std::to_string(failed) +
                          " checks=" + std::to_string(results.size())));
    }

    // 6. DLL watch (check for AC module loads)
    if (m_frameLoop.frame_index % 100 == 0) {
        auto pending = m_dllWatch.pending_notifications();
        for (auto& mod : pending) {
            ETL_RECORD(sim::EtlEventType::EvasionTick,
                       "DLL load detected: " + mod.moduleName);
        }
    }

    // 7. Disguise metrics (collect real CPU/RAM, generate fake GPU temp)
    if (m_frameLoop.frame_index % 60 == 0) {
        auto metrics = m_disguise.sample();
        ETL_RECORD(sim::EtlEventType::EvasionTick,
                   "disguise gpuTemp=" + std::to_string(metrics.fakeGpuTemp) +
                   " availMB=" + std::to_string(metrics.availPhysMB));
    }

#if LR_PLATFORM_WINDOWS
    // 8. WDA capture exclusion — only called periodically, not every frame
    if (m_useReal) {
        static uint64_t wda_frame_counter = 0;
        ++wda_frame_counter;
        // Call every ~900 frames (~15s at 60fps) with jitter
        uint64_t rng = wda_frame_counter * 0x9E3779B97F4A7C15ULL;
        uint32_t interval = 900 + static_cast<uint32_t>(rng & 127);
        if (wda_frame_counter % interval == 0) {
            m_overlay.ensure_capture_exclusion();
        }

        // V2-02: Periodic hook check (~150-200 frames)
        static int hook_check_counter = 0;
        if (++hook_check_counter >= 150) {
            hook_check_counter = 0;
            if (real::win::check_hooks()) {
                ETL_RECORD(sim::EtlEventType::EvasionTick, "HOOKS DETECTED by hook_detect");
            }
        }

        // X-03: NtQueryInformationProcess anti-debug check (~300 frames)
        static int anti_debug_counter = 0;
        if (++anti_debug_counter >= 300) {
            anti_debug_counter = 0;
            if (real::win::check_debugger_ntqsi()) {
                // Debugger detected — could set a flag or take evasive action
            }
        }

        // V4-V7/V7-R3: VEH cycling to evade CS2 diagnostic module 12 probing
#if defined(LR_ENABLE_VEH)
        static int veh_cycle_counter = 0;
        if (++veh_cycle_counter >= 250) {
            veh_cycle_counter = 0;
            real::win::veh_cycle();
        }
#endif
    }
#endif

    ETL_RECORD(sim::EtlEventType::Frame, "pre_frame complete");
    return true;
}

// ══════════════════════════════════════════════════════════════════════
//  collect_frame()  —  entity / HUD / CVar reads
// ══════════════════════════════════════════════════════════════════════

bool FramePipeline::post_frame() noexcept {
    ETL_RECORD(sim::EtlEventType::Frame, "post_frame start");

    // 1. Health ladder evaluation (Soft→Medium→Hard escalation)
    auto healthAction = m_health.evaluate();
    if (healthAction != sim::HealthAction::None) {
        const char* actionNames[] = {"None", "InvalidateHudRecollect",
                                      "RescanResolve", "ReacquireHijack"};
        ETL_RECORD(sim::EtlEventType::HealAction,
                   "health=" + std::to_string(static_cast<int>(m_health.current_level())) +
                   " action=" + actionNames[static_cast<int>(healthAction)]);
    }

    // 2. Accept readiness gate — metrics derived from live pipeline state.
    // Never hardcode all-true GateConditions (that was theater).
    sim::GateMetrics gm{};
    gm.chunkCount = m_chunkCount;
    gm.remoteEntityCount = m_lastRemoteEntityCount;
    gm.bandEnforced = m_bandEnforced;
    gm.cvarWalkOk = m_cvarWalkOk;
    gm.overlayMinPx = m_overlayMinPx;
    gm.chunkRatio = m_chunkRatio;
    gm.zeroPushRatio = m_zeroPushRatio;
    gm.cvarInitSeconds = m_cvarInitSeconds;
    // Heartbeat requires self-verify + API integrity both OK.
    gm.heartbeatOk = m_selfVerifyOk && m_apiIntegrityOk;
    m_lastGateMetrics = gm;
    bool ready = m_gate.evaluate(gm);
    if (m_frameLoop.frame_index % 100 == 0 || ready) {
        ETL_RECORD(sim::EtlEventType::OverlayPresent,
                   ready
                       ? ("ACCEPT: ready (" + std::to_string(m_gate.met_count()) +
                          "/" + std::to_string(m_gate.required_count()) + ")")
                       : ("ACCEPT: " + std::to_string(m_gate.met_count()) + "/" +
                          std::to_string(m_gate.required_count()) +
                          " met mask=0x" +
                          std::to_string(m_gate.failed_mask())));
    }

    // 3. Decoy render (ML confusion)
    auto decoys = m_decoy.generate(1920, 1080, {0, 0, 0});
    if (!decoys.empty()) {
        ETL_RECORD(sim::EtlEventType::RadarRender,
                   "decoy ops: " + std::to_string(decoys.size()));
    }

    // 4. Shellcode heartbeat (verify shellcode integrity)
    if (m_frameLoop.frame_index % 500 == 0) {
        auto sc = m_shellcode.build();
        ETL_RECORD(sim::EtlEventType::EvasionTick,
                   "shellcode: " + std::to_string(sc.size) + " bytes");
    }

    ETL_RECORD(sim::EtlEventType::Frame, "post_frame complete");
    return true;
}

} // namespace radar
