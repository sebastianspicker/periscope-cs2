// radar_pipeline.hpp — Complete radar frame pipeline wiring ALL features.
//
// This is the integration point that connects every orphaned ac_sim and
// periscope feature into a coherent radar frame loop. All 15+ ac_sim
// classes are instantiated and wired here.
//
// Previously all 15 ac_sim classes were fully implemented but NEVER
// instantiated or called from any pipeline. This file resolves that.
//
// Frame flow:
//   pre_frame() - init evasion, batch engine, RNG, disguise
//   collect()   - entity/HUD/CVar reads via hijack
//   post_frame() - temporal jitter, health check, accept gate, decoy, forensic cleanup
//
// Reference: RADAR_IMPLEMENTATION_LEDGER.md §8 Pipeline Integration
//            Periscope prototype/src/radar_pipeline.cpp

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include "subsystem_handle.hpp"
#include "init_orchestrator.hpp"
#include "frame_loop.hpp"

#include "ac_sim/batch_engine.hpp"
#include "ac_sim/etl.hpp"
#include "ac_sim/temporal.hpp"
#include "ac_sim/decoy_render.hpp"
#include "ac_sim/health_ladder.hpp"
#include "ac_sim/accept_gate.hpp"
#include "ac_sim/xorshift.hpp"
#include "ac_sim/disguise.hpp"
#include "ac_sim/forensic.hpp"
#include "ac_sim/api_integrity.hpp"
#include "ac_sim/system_normalize.hpp"
#include "ac_sim/behavioral_filter.hpp"
#include "ac_sim/dll_watch.hpp"
#include "ac_sim/self_verify.hpp"
#if LR_HAS_REAL_SHELLCODE
#include "real/shellcode/shellcode.hpp"
#endif

#if LR_PLATFORM_WINDOWS
#include "real/cs2/hijack_reader.hpp"
#include "real/cs2/periscope_entity.hpp"
#include "real/cs2/periscope_hud.hpp"
#include "real/cs2/periscope_radar.hpp"
#include "real/win/api_table.hpp"
#include "real/cs2/radar.hpp"            // RadarSnapshot, entities_to_blips
#include "real/gpu/periscope_overlay.hpp"
#endif

#include <cstdint>
#include <cstdio>
#include <string>

namespace radar::real_adapter {

/// Complete radar frame pipeline that integrates ALL features.
/// Every ac_sim class is instantiated and wired into the frame loop.
class RealFramePipeline {
public:
    RealFramePipeline() noexcept;
    ~RealFramePipeline() noexcept;

    /// Initialize ALL subsystems.
    bool initialize(uint32_t cs2_pid, bool use_real_cs2) noexcept;

    /// Frame N: execute the complete radar loop.
    /// Returns true if the frame completed successfully.
    bool run_frame() noexcept;

    /// Shutdown all subsystems cleanly.
    void shutdown() noexcept;

    // ── Accessors (for monitoring / diagnostics) ────────────────────
    const sim::ExecutionTimeline& timeline() const noexcept { return m_etl; }
    const sim::HealthLadder& health() const noexcept { return m_health; }
    const sim::AcceptGate& gate() const noexcept { return m_gate; }
    const sim::BehavioralFilter& behavioral() const noexcept { return m_behavioral; }
    const sim::ApiIntegrityChecker& api_integrity() const noexcept {
        return m_apiIntegrity;
    }
    uint64_t frame_count() const noexcept { return m_frameLoop.frame_index; }
    /// Last accept-gate metrics fed into evaluate() (not hardcoded theater).
    const sim::GateMetrics& last_gate_metrics() const noexcept {
        return m_lastGateMetrics;
    }
    bool last_api_integrity_ok() const noexcept { return m_apiIntegrityOk; }
    int last_entity_count() const noexcept { return m_lastEntityCount; }
    int last_filtered_entity_count() const noexcept {
        return m_lastFilteredEntityCount;
    }
    bool api_integrity_registry_ready() const noexcept {
        return m_apiIntegrityRegistryReady;
    }

private:
    // ── All 15 ac_sim features wired here ───────────────────────────
    sim::ExecutionTimeline    m_etl;
    sim::BatchEngine          m_batch;
    sim::TemporalEngine       m_temporal;
    sim::DecoyRenderEngine    m_decoy;
    sim::HealthLadder         m_health;
    sim::AcceptGate           m_gate;
    sim::XorShiftPool         m_rng;
    sim::DisguiseEngine       m_disguise;
    sim::ForensicEngine       m_forensic;
    sim::ApiIntegrityChecker  m_apiIntegrity;
    sim::SystemNormalizer     m_sysNorm;
    sim::BehavioralFilter     m_behavioral;
    sim::DllWatch             m_dllWatch;
    sim::SelfVerify           m_selfVerify;
#if LR_HAS_REAL_SHELLCODE
    real::shellcode::ShellcodeEngine m_shellcode;
#endif

#if LR_PLATFORM_WINDOWS
    // ── Real/periscope features wired here ───────────────────────────
    real::cs2::hijack::HijackReader       m_hijack;
    real::cs2::periscope::EntityCollector m_entityCollector;
    real::cs2::periscope::HudRadarReader  m_hudReader;
    real::cs2::periscope::CvarManager     m_cvarMgr;
    real::gpu::periscope::StealthOverlay  m_overlay;
    real::gpu::periscope::DxgiComposite   m_dxgi;     // <-- WAS orphaned, NOW wired
#endif

    // ── Subsystem lifecycle handles ────────────────────────────────
    static constexpr std::size_t kMaxHandles = 22;
    real::SubsystemHandle m_handles[kMaxHandles]{};
    std::size_t m_handleCount = 0;

    // ── Orchestration and frame loop ────────────────────────────────
    real::InitOrchestrator m_orchestrator;
    real::FrameLoopPhase   m_phase{};
    real::FrameLoop        m_frameLoop{};

    bool     m_initialized{};
    bool     m_useReal{};
    uint32_t m_cs2Pid{};
    uint64_t m_warmupFrame{};  // V6-B1/V7-I1: read ramp-up counter, reset on death/unfocus

    // ── Live accept-gate / integrity state (updated each frame) ─────
    sim::GateMetrics m_lastGateMetrics{};
    bool m_apiIntegrityOk{false};
    bool m_apiIntegrityRegistryReady{false};
    bool m_selfVerifyOk{true};
    bool m_cvarWalkOk{false};
    bool m_bandEnforced{false};
    int  m_overlayMinPx{0};
    int  m_lastEntityCount{0};
    int  m_lastRemoteEntityCount{0};
    int  m_lastFilteredEntityCount{0};
    int  m_chunkCount{0};
    float m_chunkRatio{0.f};
    float m_zeroPushRatio{1.f};
    float m_cvarInitSeconds{1e9f};
    uint64_t m_cvarFirstOkFrame{0};
    uint64_t m_emptyEntityFrames{0};
    uint64_t m_entitySampleFrames{0};

    // ── Subsystem init/shutdown stubs (static, defined in .cpp) ─────
    static bool  init_rng() noexcept;
    static bool  init_temporal() noexcept;
    static bool  init_batch() noexcept;
    static bool  init_disguise() noexcept;
    static bool  init_behavioral() noexcept;
    static bool  init_health() noexcept;
    static bool  init_gate() noexcept;
    static bool  init_self_verify() noexcept;
    static bool  init_sys_norm() noexcept;
    static bool  init_api_integrity() noexcept;
    static bool  init_dll_watch() noexcept;
    static bool  init_shellcode() noexcept;
    static bool  init_forensic() noexcept;
    static bool  init_decoy() noexcept;
    static bool  init_etl() noexcept;
    static bool  init_hijack() noexcept;
    static bool  init_overlay() noexcept;
    static bool  init_dxgi() noexcept;
    static bool  init_platform_core() noexcept;

    static void  shutdown_rng() noexcept;
    static void  shutdown_temporal() noexcept;
    static void  shutdown_batch() noexcept;
    static void  shutdown_disguise() noexcept;
    static void  shutdown_behavioral() noexcept;
    static void  shutdown_health() noexcept;
    static void  shutdown_gate() noexcept;
    static void  shutdown_self_verify() noexcept;
    static void  shutdown_sys_norm() noexcept;
    static void  shutdown_api_integrity() noexcept;
    static void  shutdown_dll_watch() noexcept;
    static void  shutdown_shellcode() noexcept;
    static void  shutdown_forensic() noexcept;
    static void  shutdown_decoy() noexcept;
    static void  shutdown_etl() noexcept;
    static void  shutdown_hijack() noexcept;
    static void  shutdown_overlay() noexcept;
    static void  shutdown_dxgi() noexcept;
    static void  shutdown_platform_core() noexcept;

    /// Mark the registered subsystem handle (by name) as shut down.
    /// The handle is looked up in m_handles; used by the shutdown stubs.
    static void mark_handle_shutdown(const char* name) noexcept;

    // ── Frame phase stubs (static, defined in .cpp) ─────────────────
    static bool  pre_phase_stub(uint64_t frame) noexcept;
    static bool  collect_phase_stub(uint64_t frame) noexcept;
    static bool  post_phase_stub(uint64_t frame) noexcept;

    // ── Helpers called by stubs ─────────────────────────────────────
    bool pre_frame() noexcept;
    bool collect_frame() noexcept;
    bool post_frame() noexcept;
};

} // namespace radar::real_adapter
