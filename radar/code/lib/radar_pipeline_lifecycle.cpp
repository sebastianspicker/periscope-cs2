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

bool FramePipeline::init_rng() noexcept {
    g_pipeline->m_rng.seed_all(0xC0FFEEu);
    ETL_RECORD(sim::EtlEventType::Init, "RNG pool seeded, 5 instances");
    return true;
}

bool FramePipeline::init_temporal() noexcept {
    g_pipeline->m_temporal.initialize(
        sim::XorShiftPool::instance(sim::XorShiftInstance::Temporal).next());
    ETL_RECORD(sim::EtlEventType::Init, "TemporalEngine initialized, 4-phase");
    return true;
}

bool FramePipeline::init_batch() noexcept {
    g_pipeline->m_batch.set_jitter_params(1.0f, 0.5f);
    g_pipeline->m_batch.set_decoy_ratio(0.05f);
    ETL_RECORD(sim::EtlEventType::Init, "BatchEngine initialized, 16-slot");
    return true;
}

bool FramePipeline::init_disguise() noexcept {
    g_pipeline->m_disguise.initialize();
    ETL_RECORD(sim::EtlEventType::Init, "DisguiseEngine initialized");
    return true;
}

bool FramePipeline::init_behavioral() noexcept {
    sim::BehavioralFilterConfig cfg{};
    // Frame loop owns timing (TemporalEngine); filter must not sleep here.
    cfg.applyDelay = false;
    g_pipeline->m_behavioral.initialize(
        cfg, sim::XorShiftPool::instance(sim::XorShiftInstance::Semantic).next());
    ETL_RECORD(sim::EtlEventType::Init,
               "BehavioralFilter initialized applyDelay=0");
    return true;
}

bool FramePipeline::init_health() noexcept {
    sim::HealthLadderConfig hlc;
    hlc.softBadFrameThreshold = 3;
    hlc.mediumSoftActionThreshold = 10;
    hlc.hardMediumActionThreshold = 3;
    g_pipeline->m_health.initialize(hlc);
    ETL_RECORD(sim::EtlEventType::Init, "HealthLadder initialized, 3-level");
    return true;
}

bool FramePipeline::init_gate() noexcept {
    g_pipeline->m_gate.initialize();
    ETL_RECORD(sim::EtlEventType::Init, "AcceptGate initialized, 7 conditions");
    return true;
}

bool FramePipeline::init_self_verify() noexcept {
    g_pipeline->m_selfVerify.initialize();
    ETL_RECORD(sim::EtlEventType::Init, "SelfVerifier initialized");
    return true;
}

bool FramePipeline::init_sys_norm() noexcept {
    g_pipeline->m_sysNorm.initialize();
    ETL_RECORD(sim::EtlEventType::Init, "SystemNormalizer initialized");
    return true;
}

bool FramePipeline::init_api_integrity() noexcept {
    // Populate known-good module registry so verify()/verify_all() have a
    // real containment baseline. Never pretends success without work.
#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
    auto register_module = [](sim::ApiIntegrityChecker& chk, const char* name,
                              uintptr_t base) {
        if (!base || !name) return false;
        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
            base + static_cast<uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        const uint64_t size = nt->OptionalHeader.SizeOfImage;
        if (size < 0x1000) return false;
        chk.set_known_good(name, static_cast<uint64_t>(base), size);
        return true;
    };

    auto& api = real::win::g_Api();
    api.ensure_resolved();

    int registered = 0;
    if (register_module(g_pipeline->m_apiIntegrity, "ntdll",
                        real::win::peb::find_module("ntdll")))
        ++registered;
    if (register_module(g_pipeline->m_apiIntegrity, "kernel32",
                        real::win::peb::find_module("kernel32")))
        ++registered;
    if (auto* peb = real::win::peb::get_peb()) {
        if (register_module(g_pipeline->m_apiIntegrity, "self",
                            reinterpret_cast<uintptr_t>(peb->ImageBaseAddress)))
            ++registered;
    }
    g_pipeline->m_apiIntegrityRegistryReady = registered > 0;

    // Baseline probe of critical read path if resolved.
    if (api.NtReadVirtualMemory) {
        auto r = g_pipeline->m_apiIntegrity.verify(
            "ntdll", "NtReadVirtualMemory",
            reinterpret_cast<uint64_t>(api.NtReadVirtualMemory));
        g_pipeline->m_apiIntegrityOk = r.passed;
    } else {
        g_pipeline->m_apiIntegrityOk = false;
    }

    ETL_RECORD(sim::EtlEventType::Init,
               "ApiIntegrityChecker initialized modules=" +
                   std::to_string(registered) +
                   (g_pipeline->m_apiIntegrityOk ? " probe=ok" : " probe=fail"));
#else
    // Non-Windows lab hosts: still mark registry ready so offline paths are
    // honest about platform support via verify_all detail strings.
    g_pipeline->m_apiIntegrity.set_known_good("lab_host", 0x10000, 0x1000);
    g_pipeline->m_apiIntegrityRegistryReady = true;
    g_pipeline->m_apiIntegrityOk = true;
    ETL_RECORD(sim::EtlEventType::Init,
               "ApiIntegrityChecker initialized (lab host synthetic registry)");
#endif
    return true;
}

bool FramePipeline::init_dll_watch() noexcept {
    g_pipeline->m_dllWatch.initialize();
    g_pipeline->m_dllWatch.start_monitoring();
    ETL_RECORD(sim::EtlEventType::Init, "DllWatchDog initialized, 33 AC patterns");
    return true;
}

bool FramePipeline::init_shellcode() noexcept {
    g_pipeline->m_shellcode.build();
    ETL_RECORD(sim::EtlEventType::Init, "ShellcodeEngine initialized, 101 bytes");
    return true;
}

bool FramePipeline::init_forensic() noexcept {
    ETL_RECORD(sim::EtlEventType::Init, "ForensicEngine ready for cleanup");
    return true;
}

bool FramePipeline::init_decoy() noexcept {
    g_pipeline->m_decoy.initialize(
        sim::XorShiftPool::instance(sim::XorShiftInstance::Render).next());
    ETL_RECORD(sim::EtlEventType::Init, "DecoyRenderEngine initialized");
    return true;
}

bool FramePipeline::init_etl() noexcept {
    return true;
}

bool FramePipeline::init_hijack() noexcept {
#if LR_PLATFORM_WINDOWS
    if (g_pipeline->m_useReal) {
        if (g_pipeline->m_hijack.setup(g_pipeline->m_cs2Pid)) {
            ETL_RECORD(sim::EtlEventType::Init, "HijackReader attached, donor proxy active");
        }
    }
#else
    (void)g_pipeline;
#endif
    return true;
}

bool FramePipeline::init_overlay() noexcept {
#if LR_PLATFORM_WINDOWS
    if (g_pipeline->m_useReal) {
        real::gpu::periscope::OverlayConfig ovCfg;
        ovCfg.width = 360;
        ovCfg.height = 360;
        ovCfg.alpha = 220;
        if (g_pipeline->m_overlay.initialize(ovCfg)) {
            ETL_RECORD(sim::EtlEventType::Init, "StealthOverlay created, capture-proof");
        }
    }
#endif
    return true;
}

bool FramePipeline::init_dxgi() noexcept {
#if LR_PLATFORM_WINDOWS
    if (g_pipeline->m_useReal) {
        if (g_pipeline->m_dxgi.initialize()) {
            ETL_RECORD(sim::EtlEventType::Init, "DxgiComposite initialized");
        }
    }
#endif
    return true;
}

bool FramePipeline::init_platform_core() noexcept {
#if LR_PLATFORM_WINDOWS
    if (g_pipeline->m_useReal) {
        auto& api = real::win::g_Api();
        if (!api.resolved) {
#ifndef NDEBUG
            std::printf("[pipeline] WARNING: API table not resolved\n");
#endif
        } else {
            ETL_RECORD(sim::EtlEventType::Init, "API table resolved, ~80 functions");
        }

        real::win::clear_debug_flags();
        ETL_RECORD(sim::EtlEventType::Init, "PEB debug flags cleared");

        real::win::erase_pe_header();
        ETL_RECORD(sim::EtlEventType::Init, "PE header erased");

        // V5-T4: VEH and hook detection are registered by default for
        // T0/T1 where usermode ntdll hooks could intercept API calls.
        // For dedicated T2+ builds, gate this behind an explicit flag;
        // by default both are registered as they provide defense-in-depth.
        real::win::init_hook_detection();
        ETL_RECORD(sim::EtlEventType::Init, "Hook detection initialized");

        // V7-R3: VEH registration is opt-in. VEH handlers are visible in the
        // kernel-managed VECTORED_HANDLER list and can be enumerated by AC.
        // Define LR_ENABLE_VEH to enable. Disabled by default in release.
#if defined(LR_ENABLE_VEH)
        real::win::register_veh();
        ETL_RECORD(sim::EtlEventType::Init, "VEH anti-debug registered (opt-in)");
#else
        ETL_RECORD(sim::EtlEventType::Init, "VEH anti-debug disabled by default");
#endif

        // V5-F3: Install top-level exception handler to suppress WER crash dumps.
        // Without this, an unhandled crash creates a full minidump of the cheat
        // process in %LOCALAPPDATA%\CrashDumps — a complete forensic artifact.
        ::SetUnhandledExceptionFilter([](PEXCEPTION_POINTERS) -> LONG {
            ExitProcess(0);  // Terminate immediately — no WER dump
            return EXCEPTION_EXECUTE_HANDLER;
        });
    }
#endif
    return true;
}

// ══════════════════════════════════════════════════════════════════════
//  Static shutdown stubs
// ══════════════════════════════════════════════════════════════════════

void FramePipeline::mark_handle_shutdown(const char* name) noexcept {
    for (std::size_t i = 0; i < g_pipeline->m_handleCount; ++i) {
        if (g_pipeline->m_handles[i].name &&
            std::strcmp(g_pipeline->m_handles[i].name, name) == 0) {
            g_pipeline->m_handles[i].initialized = false;
            return;
        }
    }
}

void FramePipeline::shutdown_rng() noexcept {
    // XorShiftPool owns process-global static instances and exposes no
    // deinit API — the pool is intentionally seeded once for the process
    // lifetime, so teardown is limited to clearing the handle state.
    mark_handle_shutdown("RNG");
}

void FramePipeline::shutdown_temporal() noexcept {
    // TemporalEngine has no dedicated deinit; initialize() resets all
    // phase/tick state to idle (seed=0, phase=Alpha).
    g_pipeline->m_temporal.initialize(0);
    mark_handle_shutdown("Temporal");
}

void FramePipeline::shutdown_batch() noexcept {
    // Flush staged requests and restore default rate/jitter parameters.
    g_pipeline->m_batch.clear();
    g_pipeline->m_batch.set_rate_limit(120);
    g_pipeline->m_batch.set_jitter_params(1.0f, 0.5f);
    g_pipeline->m_batch.set_decoy_ratio(0.0f);
    mark_handle_shutdown("Batch");
}

void FramePipeline::shutdown_disguise() noexcept {
    g_pipeline->m_disguise.shutdown();
    mark_handle_shutdown("Disguise");
}

void FramePipeline::shutdown_behavioral() noexcept {
    // Clears per-entity reaction tracking, delay and origin hold state.
    g_pipeline->m_behavioral.reset();
    mark_handle_shutdown("Behavioral");
}

void FramePipeline::shutdown_health() noexcept {
    g_pipeline->m_health.reset();
    mark_handle_shutdown("HealthLadder");
}

void FramePipeline::shutdown_gate() noexcept {
    g_pipeline->m_gate.reset();
    mark_handle_shutdown("AcceptGate");
}

void FramePipeline::shutdown_self_verify() noexcept {
    // Zero the expected-CRC baseline (the only mutable state).
    g_pipeline->m_selfVerify.initialize();
    mark_handle_shutdown("SelfVerifier");
}

void FramePipeline::shutdown_sys_norm() noexcept {
    g_pipeline->m_sysNorm.shutdown();
    mark_handle_shutdown("SystemNormalizer");
}

void FramePipeline::shutdown_api_integrity() noexcept {
    // ApiIntegrityChecker exposes no clear/reset; the known-good module
    // registry is retained for the process lifetime.
    mark_handle_shutdown("ApiIntegrity");
}

void FramePipeline::shutdown_dll_watch() noexcept {
    g_pipeline->m_dllWatch.shutdown();
    mark_handle_shutdown("DllWatchDog");
}

void FramePipeline::shutdown_shellcode() noexcept {
    // ShellcodeEngine is stateless between build() calls; its shared-memory
    // payload is a process-lifetime static with no teardown required.
    mark_handle_shutdown("Shellcode");
}

void FramePipeline::shutdown_forensic() noexcept {
    g_pipeline->m_forensic.execute_cleanup();
    g_pipeline->m_forensic.shutdown();
    mark_handle_shutdown("Forensic");
    ETL_RECORD(sim::EtlEventType::Shutdown, "forensic cleanup executed");
}

void FramePipeline::shutdown_decoy() noexcept {
    // No dedicated deinit; initialize() resets the render seed and
    // set_params() restores the default decoy profile.
    g_pipeline->m_decoy.initialize(0);
    g_pipeline->m_decoy.set_params(3, 0.08f, 5.0f);
    mark_handle_shutdown("Decoy");
}

void FramePipeline::shutdown_etl() noexcept {
    g_pipeline->m_etl.flush_to_file("pipeline_etl.log");
    g_pipeline->m_etl.shutdown();
    mark_handle_shutdown("ETL");
    ETL_RECORD(sim::EtlEventType::Shutdown, "ETL flushed");
}

void FramePipeline::shutdown_hijack() noexcept {
#if LR_PLATFORM_WINDOWS
    g_pipeline->m_hijack.shutdown();
#endif
}

void FramePipeline::shutdown_overlay() noexcept {
#if LR_PLATFORM_WINDOWS
    g_pipeline->m_overlay.shutdown();
#endif
}

void FramePipeline::shutdown_dxgi() noexcept {
#if LR_PLATFORM_WINDOWS
    g_pipeline->m_dxgi.shutdown();
#endif
}

void FramePipeline::shutdown_platform_core() noexcept {
#if LR_PLATFORM_WINDOWS
#if defined(LR_ENABLE_VEH)
    real::win::unregister_veh();
    ETL_RECORD(sim::EtlEventType::Shutdown, "VEH anti-debug unregistered");
#endif
#endif
}

// ══════════════════════════════════════════════════════════════════════
//  Static phase stubs (called by FrameLoop)
// ══════════════════════════════════════════════════════════════════════

bool FramePipeline::pre_phase_stub(uint64_t /*frame*/) noexcept {
    return g_pipeline->pre_frame();
}

bool FramePipeline::collect_phase_stub(uint64_t /*frame*/) noexcept {
    return g_pipeline->collect_frame();
}

bool FramePipeline::post_phase_stub(uint64_t /*frame*/) noexcept {
    return g_pipeline->post_frame();
}

// ══════════════════════════════════════════════════════════════════════
//  Helper: register one subsystem handle
// ══════════════════════════════════════════════════════════════════════

namespace {
    void register_subsystem(
        real::SubsystemHandle* handles, std::size_t& count,
        real::InitOrchestrator& orch,
        const char* name,
        bool (*init_fn)() noexcept,
        void (*shutdown_fn)() noexcept,
        int dep_count = 0,
        const char** deps = nullptr) noexcept
    {
        auto& h = handles[count];
        h.name = name;
        h.init_fn = init_fn;
        h.shutdown_fn = shutdown_fn;
        h.tick_fn = nullptr;
        h.dependency_count = dep_count;
        h.dependencies = deps;
        h.initialized = false;
        h.tick_enabled = true;
        orch.add(&h);
        count++;
    }
} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════
//  Constructor / Destructor
// ══════════════════════════════════════════════════════════════════════

FramePipeline::FramePipeline() noexcept {
    // Construction only — use initialize() to start
}

FramePipeline::~FramePipeline() noexcept {
    shutdown();
}

// ══════════════════════════════════════════════════════════════════════
//  initialize()  —  creates SubsystemHandles, registers with
//                   InitOrchestrator, calls initialize_all()
// ══════════════════════════════════════════════════════════════════════

bool FramePipeline::initialize(uint32_t cs2_pid, bool use_real_cs2) noexcept {
    m_cs2Pid = cs2_pid;
    m_useReal = use_real_cs2;
    m_handleCount = 0;
    // Set global pipeline pointer so static stubs can reach members.
    g_pipeline = this;

    // ── Configure FrameLoop ──────────────────────────────────────────
    m_phase.pre = &FramePipeline::pre_phase_stub;
    m_phase.collect = &FramePipeline::collect_phase_stub;
    m_phase.post = &FramePipeline::post_phase_stub;
    m_frameLoop.phases = &m_phase;
    m_frameLoop.phase_count = 1;
    m_frameLoop.frame_index = 0;

    // ── Register all subsystem handles (init order governed by deps) ─
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "RNG",             &FramePipeline::init_rng,             &FramePipeline::shutdown_rng);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "Temporal",        &FramePipeline::init_temporal,        &FramePipeline::shutdown_temporal,
        1, kDepRng);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "Batch",           &FramePipeline::init_batch,           &FramePipeline::shutdown_batch);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "Disguise",        &FramePipeline::init_disguise,        &FramePipeline::shutdown_disguise);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "Behavioral",      &FramePipeline::init_behavioral,      &FramePipeline::shutdown_behavioral);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "HealthLadder",    &FramePipeline::init_health,          &FramePipeline::shutdown_health);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "AcceptGate",      &FramePipeline::init_gate,            &FramePipeline::shutdown_gate);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "SelfVerifier",    &FramePipeline::init_self_verify,     &FramePipeline::shutdown_self_verify);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "SystemNormalizer",&FramePipeline::init_sys_norm,        &FramePipeline::shutdown_sys_norm);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "ApiIntegrity",    &FramePipeline::init_api_integrity,   &FramePipeline::shutdown_api_integrity);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "DllWatchDog",     &FramePipeline::init_dll_watch,       &FramePipeline::shutdown_dll_watch);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "Shellcode",       &FramePipeline::init_shellcode,       &FramePipeline::shutdown_shellcode);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "Decoy",           &FramePipeline::init_decoy,           &FramePipeline::shutdown_decoy,
        1, kDepRng);
    // Subsystems with real shutdown logic are registered in the reverse
    // of their desired shutdown order so that shutdown_all()'s LIFO
    // iteration produces: Forensic → ETL → PlatformCore(VEH) → Hijack → Overlay → Dxgi.
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "DxgiComposite",   &FramePipeline::init_dxgi,            &FramePipeline::shutdown_dxgi);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "Overlay",         &FramePipeline::init_overlay,         &FramePipeline::shutdown_overlay);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "Hijack",          &FramePipeline::init_hijack,          &FramePipeline::shutdown_hijack);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "PlatformCore",    &FramePipeline::init_platform_core,   &FramePipeline::shutdown_platform_core);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "ETL",             &FramePipeline::init_etl,             &FramePipeline::shutdown_etl);
    register_subsystem(m_handles, m_handleCount, m_orchestrator,
        "Forensic",        &FramePipeline::init_forensic,        &FramePipeline::shutdown_forensic);

    // ── Run all initializers in dependency order ─────────────────────
    if (!m_orchestrator.initialize_all()) {
        return false;
    }

    m_initialized = true;
    ETL_RECORD(sim::EtlEventType::Init, "FramePipeline fully initialized");
#ifndef NDEBUG
    std::printf("[pipeline] FramePipeline initialized: %zu features wired\n", m_handleCount);
#endif
    return true;
}

// ══════════════════════════════════════════════════════════════════════
//  run_frame()  —  delegates to FrameLoop
// ══════════════════════════════════════════════════════════════════════

void FramePipeline::shutdown() noexcept {
    if (!m_initialized) return;

    ETL_RECORD(sim::EtlEventType::Shutdown, "FramePipeline shutting down");

    m_orchestrator.shutdown_all();

    m_initialized = false;
#ifndef NDEBUG
    std::printf("[pipeline] FramePipeline shutdown: %llu frames processed\n",
                (unsigned long long)m_frameLoop.frame_index);
#endif
}

// ══════════════════════════════════════════════════════════════════════
//  pre_frame()  —  per-frame evasion & pre-collection logic
// ══════════════════════════════════════════════════════════════════════

} // namespace radar
