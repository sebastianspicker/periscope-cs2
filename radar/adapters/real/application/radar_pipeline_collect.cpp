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

namespace radar::real_adapter {

using FramePipeline = RealFramePipeline;

// Shared pipeline instance for static InitOrchestrator stubs (one definition).
extern FramePipeline* g_pipeline;
// Dependency-name arrays for subsystems that require ordering.
extern const char* kDepRng[];

bool FramePipeline::collect_frame() noexcept {
    ETL_RECORD(sim::EtlEventType::Frame, "collect_frame start");

#if LR_PLATFORM_WINDOWS
    // Lab / offline path: still exercise BehavioralFilter + gate metrics so
    // the phase is never an orphaned no-op that pretends success.
    if (!m_useReal) {
        ac::EntitySnapshot lab[4]{};
        lab[0] = ac::EntitySnapshot(1, {0.f, 0.f, 0.f}, 2, true);
        lab[0].is_local_player = true;
        lab[0].health = 100;
        lab[1] = ac::EntitySnapshot(2, {12.f, 4.f, 0.f}, 3, true);
        lab[1].health = 90;
        lab[2] = ac::EntitySnapshot(3, {40.f, -8.f, 0.f}, 3, true);
        lab[2].health = 70;
        lab[3] = ac::EntitySnapshot(4, {90.f, 20.f, 0.f}, 3, true);
        lab[3].health = 50;
        const ac::Vec3 localOrigin{0.f, 0.f, 0.f};
        // Filter in place — applyDelay already false from init_behavioral.
        m_behavioral.filter_entities(lab, 4, localOrigin, /*speed=*/0.f,
                                     /*occluded=*/false, /*yaw_h=*/0.f,
                                     /*yaw_v=*/0.f);
        int alive = 0;
        int remote = 0;
        for (const auto& e : lab) {
            if (e.alive && !e.dormant) {
                ++alive;
                if (!e.is_local_player) ++remote;
            }
        }
        m_lastEntityCount = 4;
        m_lastRemoteEntityCount = remote;
        m_lastFilteredEntityCount = alive;
        m_chunkCount = alive >= 2 ? 2 : (alive > 0 ? 1 : 0);
        m_chunkRatio = alive / 4.f;
        ++m_entitySampleFrames;
        if (alive <= 1) ++m_emptyEntityFrames;
        m_zeroPushRatio =
            m_entitySampleFrames
                ? static_cast<float>(m_emptyEntityFrames) /
                      static_cast<float>(m_entitySampleFrames)
                : 1.f;
        // Offline lab has no real cvar/overlay/band — keep honest zeros.
        m_cvarWalkOk = false;
        m_bandEnforced = false;
        m_overlayMinPx = 0;
        m_cvarInitSeconds = 1e9f;
        ETL_RECORD(sim::EtlEventType::EntityCollect,
                   "lab filter entities alive=" + std::to_string(alive) +
                       " omitted=" +
                       std::to_string(m_behavioral.stats().entitiesOmitted));
        m_health.reset_bad_frames();
        return true;
    }

    // V5-B2: Gate reads on game window focus — skip entity reads
    // when the game window is not in the foreground.
    // Uses direct Win32 calls since this is a UI process with user32 loaded.
    static HWND s_cachedGameHwnd = nullptr;
    if (!s_cachedGameHwnd) {
        s_cachedGameHwnd = ::FindWindowA("Valve001", nullptr);
    }
    if (s_cachedGameHwnd && ::GetForegroundWindow() != s_cachedGameHwnd) {
        // Game not focused — skip entity reads to avoid focus correlation detection
        m_warmupFrame = 0;  // V7-I1: Reset ramp — will restart from 5Hz on re-focus
        ETL_RECORD(sim::EtlEventType::EvasionTick, "frame skipped: game not focused");
        m_health.reset_bad_frames();
        return true;
    }

    // V5-B5: Temporal timing gate — never read faster than game tick rate
    static uint64_t s_nextReadTick = 0;
    uint64_t now = __rdtsc();
    if (now < s_nextReadTick) {
        ETL_RECORD(sim::EtlEventType::EvasionTick, "read skipped: temporal gate");
        m_health.reset_bad_frames();
        return true;
    }
    // Base interval: ~16ms (64Hz tick). Apply temporal phase jitter.
    uint64_t intervalCycles = static_cast<uint64_t>(
        m_temporal.next_delay_us() * 2.5); // ~2.5 GHz TSC
    if (intervalCycles < 40000000ULL) intervalCycles = 40000000ULL; // min 16ms
    s_nextReadTick = now + intervalCycles;

    // V5-B1/V7-I3: Local player alive state is checked after entity collect
    // (health is only known once offsets are resolved). Skip nothing here.

    // V6-B1/V7-I1: Read ramp-up — exponential warmup from 5Hz to full rate.
    // Resets on death or unfocus (reset above). Reaching here means active.
    ++m_warmupFrame;
    if (m_warmupFrame < 5) {
        // Frames 0-4: read at 5Hz (200ms interval)
        real::win::fuzzed_sleep(200, 10);
    } else     if (m_warmupFrame < 15) {
        // Frames 5-14: 15Hz (66ms)
        real::win::fuzzed_sleep(66, 15);
    } else if (m_warmupFrame < 30) {
        // Frames 15-29: 30Hz (33ms)
        real::win::fuzzed_sleep(33, 15);
    }
    // Frame 30+: full rate (controlled by temporal gate above)
    if (m_warmupFrame > 1000) m_warmupFrame = 1000; // prevent overflow

    // Read via hijack + batch engine.
    // EntityCollector APIs require a C function pointer (no capturing lambda).
    static real::cs2::hijack::HijackReader* s_hijack_for_read = nullptr;
    s_hijack_for_read = &m_hijack;
    auto readFn = +[](uint64_t addr, void* buf, size_t sz) -> bool {
        return s_hijack_for_read && s_hijack_for_read->read(addr, buf, sz);
    };

    // V5-B3: Resolve the real CS2 addresses via the periscope pattern
    // scanner (same API as demos/live_radar.cpp). The scans walk client.dll
    // in 64KB chunks and are far too expensive to run per frame, so the
    // results are cached here. If CS2 is not attached (no hijack handle)
    // the cache stays empty and this frame keeps the same defaults (0) as
    // before — resolution simply retries at a low cadence, never crashes.
    static uint64_t s_clientBase = 0;
    static size_t   s_clientSize = 0;
    static uint64_t s_engine2Base = 0;
    static size_t   s_engine2Size = 0;
    static uint64_t s_tier0Base = 0;
    static size_t   s_tier0Size = 0;
    static uint64_t s_entityListSlot = 0;   // address of dwEntityList global
    static uint64_t s_cHudSlot = 0;         // address of CHud slot (deref → object)
    static uint64_t s_localPawnSlot = 0;
    static bool     s_targetsResolved = false;
    static uint64_t s_resolveRetryCounter = 0;

    if (!s_targetsResolved) {
        ++s_resolveRetryCounter;
        // Retry at a low cadence until the hijack handle is live.
        if (s_resolveRetryCounter == 1 || (s_resolveRetryCounter % 512 == 0)) {
            const uint64_t procHandle =
                reinterpret_cast<uint64_t>(m_hijack.cs2_handle());
            if (real::cs2::find_client_module(m_cs2Pid, procHandle,
                                              s_clientBase, s_clientSize)) {
                real::cs2::find_module_by_basename(m_cs2Pid, procHandle,
                                                   "engine2.dll",
                                                   s_engine2Base, s_engine2Size);
                real::cs2::find_module_by_basename(m_cs2Pid, procHandle,
                                                   "tier0.dll",
                                                   s_tier0Base, s_tier0Size);

                auto results = real::cs2::periscope::scan_all_patterns(
                    s_clientBase, s_clientSize, readFn);
                for (const auto& r : results) {
                    if (!r.found || r.address == 0) continue;
                    if (r.name == "entity_list") {
                        s_entityListSlot = r.address;
                    } else if (r.name == "local_pawn") {
                        s_localPawnSlot = r.address;
                    } else if (r.name == "c_hud") {
                        s_cHudSlot = r.address;
                    } else if (s_cHudSlot == 0 &&
                               (r.name == "c_hud_fallback1" ||
                                r.name == "c_hud_fallback2")) {
                        s_cHudSlot = r.address;
                    }
                }
                s_targetsResolved = true;
                ETL_RECORD(sim::EtlEventType::PatternScan,
                           "client base resolved, patterns scanned");
            } else {
                ETL_RECORD(sim::EtlEventType::PatternScan,
                           "client module unresolved; keeping default addresses");
            }
        }
    }

    // 1. Entity collection (64-slot, dual representation, yaw fusion).
    //    The scanned entity_list slot holds a pointer to the list array;
    //    deref it once (mirrors read_entity_list_scattered in live_radar).
    uint64_t entityListAddr = 0;
    if (s_entityListSlot) {
        readFn(s_entityListSlot, &entityListAddr, sizeof(entityListAddr));
    }
    size_t entityCount = m_entityCollector.collect(entityListAddr, readFn);
    ETL_RECORD(sim::EtlEventType::EntityCollect,
               "entities=" + std::to_string(entityCount));

    // 2. HUD radar snapshot (BST walk for CCSGO_HudRadar).
    //    The c_hud pattern resolves the slot holding the CHud pointer;
    //    deref it once (mirrors demos/live_radar.cpp resolve_c_hud).
    uint64_t cHudAddr = 0;
    if (s_cHudSlot) {
        readFn(s_cHudSlot, &cHudAddr, sizeof(cHudAddr));
    }
    uint64_t clientBase = s_clientBase;
    size_t clientSize = s_clientSize;
    if (m_hudReader.update_snapshot(cHudAddr, clientBase, clientSize, readFn,
                                    /*stringScanFallback=*/false)) {
        ETL_RECORD(sim::EtlEventType::HudSnapshot,
                   "HUD radar snapshot valid");
    }

    // Local player (pawn + multi-source yaw fusion) and planted bomb.
    uint64_t localPawnAddr = 0;
    if (s_localPawnSlot) {
        readFn(s_localPawnSlot, &localPawnAddr, sizeof(localPawnAddr));
    }
    uint64_t viewAnglesAddr = clientBase
        ? clientBase + real::cs2::periscope::PeriscopeOffsets::kDwViewAngles : 0;
    uint64_t csgoInputAddr = clientBase
        ? clientBase + real::cs2::periscope::PeriscopeOffsets::kDwCSGOInput : 0;
    m_entityCollector.set_local_player(localPawnAddr, viewAnglesAddr,
                                       csgoInputAddr, readFn);
    uint64_t bombSlot = clientBase
        ? clientBase + real::cs2::periscope::PeriscopeOffsets::kDwPlantedC4 : 0;
    uint64_t bombAddr = 0;
    if (bombSlot) {
        readFn(bombSlot, &bombAddr, sizeof(bombAddr));
    }
    m_entityCollector.set_bomb_data(bombAddr, readFn);

    // 3. CVar resolution (4-tier cascade) using the resolved module bases.
    m_cvarMgr.initialize(s_engine2Base, s_engine2Size,
                         s_tier0Base, s_tier0Size,
                         clientBase, clientSize, cHudAddr, readFn);
    const int cvarPath = m_cvarMgr.values().resolutionPath;
    m_cvarWalkOk = cvarPath > 0;
    if (m_cvarWalkOk && m_cvarFirstOkFrame == 0) {
        m_cvarFirstOkFrame = m_frameLoop.frame_index ? m_frameLoop.frame_index : 1;
    }
    if (m_cvarFirstOkFrame > 0) {
        // ~60 fps estimate for gate timing condition.
        m_cvarInitSeconds =
            static_cast<float>(m_cvarFirstOkFrame) / 60.0f;
    }
    ETL_RECORD(sim::EtlEventType::CvarRead,
               "resolution path=" + std::to_string(cvarPath));

    // 4. DXGI composite (desktop frame timing)
    auto composite = m_dxgi.acquire_and_composite(0);
    if (composite) {
        ETL_RECORD(sim::EtlEventType::MemoryRead,
                   "DXGI composite: " + std::to_string(composite->acquireNs) +
                   "ns acquire, " + std::to_string(composite->copyNs) + "ns copy");
    }

    // Overlay size for accept gate (real window client area if present).
    m_overlayMinPx = 0;
    m_bandEnforced = m_useReal && m_hijack.cs2_handle() != nullptr;
    if (m_overlay.is_initialized()) {
        // Stealth overlay reports capture exclusion as "band" enforcement proxy.
        m_bandEnforced = m_bandEnforced && true;
        const int dim = (std::min)(m_overlay.width(), m_overlay.height());
        m_overlayMinPx = dim > 0 ? dim : 0;
    }

    // 5. Convert periscope players → EntitySnapshot, run BehavioralFilter,
    //    then build radar blips from the FILTERED snapshot (not raw players).
    const auto& local = m_entityCollector.local();
    std::vector<ac::EntitySnapshot> snaps;
    snaps.reserve(entityCount);
    for (const auto& p : m_entityCollector.players()) {
        if (!p.valid) continue;
        ac::EntitySnapshot e;
        e.id = static_cast<std::uint32_t>(snaps.size() + 1);
        e.origin = {p.origin.x, p.origin.y, p.origin.z};
        e.eye_angles = {p.eyeAngles.x, p.eyeAngles.y, p.eyeAngles.z};
        e.team = static_cast<std::uint8_t>(p.team);
        e.health = static_cast<std::int32_t>(p.health);
        e.alive = p.alive;
        e.dormant = p.dormant;
        e.is_local_player =
            (p.pawnAddr != 0 && p.pawnAddr == local.pawnAddr);
        e.weapon_id = p.weaponId;
        snaps.push_back(e);
    }

    const ac::Vec3 localOrigin{
        local.valid ? local.player.origin.x : 0.f,
        local.valid ? local.player.origin.y : 0.f,
        local.valid ? local.player.origin.z : 0.f};
    const float speed = local.valid
        ? std::sqrt(local.player.velocity.x * local.player.velocity.x +
                    local.player.velocity.y * local.player.velocity.y)
        : 0.f;
    // applyDelay is false from init_behavioral; filter mutates in place.
    if (!snaps.empty()) {
        m_behavioral.filter_entities(
            snaps.data(), snaps.size(), localOrigin, speed,
            /*is_occluded=*/false, local.valid ? local.yaw : 0.f, 0.f);
    }

    int remote = 0;
    int filteredAlive = 0;
    for (const auto& e : snaps) {
        if (e.alive && !e.dormant) {
            ++filteredAlive;
            if (!e.is_local_player) ++remote;
        }
    }
    m_lastEntityCount = static_cast<int>(snaps.size());
    m_lastRemoteEntityCount = remote;
    m_lastFilteredEntityCount = filteredAlive;
    m_chunkCount = filteredAlive >= 2 ? 2 : (filteredAlive > 0 ? 1 : 0);
    m_chunkRatio =
        snaps.empty() ? 0.f
                      : static_cast<float>(filteredAlive) /
                            static_cast<float>(snaps.size());
    ++m_entitySampleFrames;
    if (filteredAlive <= 1) ++m_emptyEntityFrames;
    m_zeroPushRatio =
        m_entitySampleFrames
            ? static_cast<float>(m_emptyEntityFrames) /
                  static_cast<float>(m_entitySampleFrames)
            : 1.f;

    real::cs2::RadarSnapshot radarSnap;
    radarSnap.map_origin_x = (!m_hudReader.snapshot().valid ? 0.0f :
        m_hudReader.snapshot().mapTexturePosition.x);
    radarSnap.map_origin_y = (!m_hudReader.snapshot().valid ? 0.0f :
        m_hudReader.snapshot().mapTexturePosition.y);
    radarSnap.map_scale = m_cvarMgr.values().clRadarScale;
    radarSnap.map_size = (m_hudReader.snapshot().valid &&
        m_hudReader.snapshot().maxVisibilitySquared > 0.0f)
        ? static_cast<int>(std::sqrt(m_hudReader.snapshot().maxVisibilitySquared))
        : 0;

    const float yawDeg = local.valid ? local.yaw : 0.0f;
    const float yawRad = yawDeg * (3.14159265f / 180.0f);
    const float edge = (radarSnap.map_scale > 0.2f) ? radarSnap.map_scale : 0.7f;
    const float cosYaw = std::cos(yawRad);
    const float sinYaw = std::sin(yawRad);
    radarSnap.blips.reserve(snaps.size());
    for (const auto& e : snaps) {
        // Omitted entities are marked dormant/!alive by the filter — skip them.
        if (!e.alive || e.dormant) continue;
        real::cs2::RadarBlip blip{};
        const float dx = e.origin.x - localOrigin.x;
        const float dy = e.origin.y - localOrigin.y;
        const float forward = dx * cosYaw + dy * sinYaw;
        const float right   = dx * sinYaw - dy * cosYaw;
        blip.x = right / edge;
        blip.y = -forward / edge;
        blip.height = e.origin.z;
        blip.team = e.team;
        blip.is_alive = e.alive;
        blip.is_visible = !e.dormant;
        blip.is_local = e.is_local_player;
        radarSnap.blips.push_back(blip);
    }

    ETL_RECORD(sim::EtlEventType::EntityCollect,
               "filtered blips=" + std::to_string(radarSnap.blips.size()) +
                   " raw=" + std::to_string(snaps.size()) +
                   " omitted=" +
                   std::to_string(m_behavioral.stats().entitiesOmitted));

    m_health.reset_bad_frames();
#else
    // Non-Windows: lab synthetic filter path (same as offline Windows).
    {
        ac::EntitySnapshot lab[4]{};
        lab[0] = ac::EntitySnapshot(1, {0.f, 0.f, 0.f}, 2, true);
        lab[0].is_local_player = true;
        lab[0].health = 100;
        lab[1] = ac::EntitySnapshot(2, {12.f, 4.f, 0.f}, 3, true);
        lab[1].health = 90;
        lab[2] = ac::EntitySnapshot(3, {40.f, -8.f, 0.f}, 3, true);
        lab[2].health = 70;
        lab[3] = ac::EntitySnapshot(4, {90.f, 20.f, 0.f}, 3, true);
        lab[3].health = 50;
        m_behavioral.filter_entities(lab, 4, {0.f, 0.f, 0.f}, 0.f, false, 0.f,
                                     0.f);
        m_lastEntityCount = 4;
        m_lastFilteredEntityCount = 0;
        for (const auto& e : lab) {
            if (e.alive && !e.dormant) ++m_lastFilteredEntityCount;
        }
        m_lastRemoteEntityCount =
            m_lastFilteredEntityCount > 0 ? m_lastFilteredEntityCount - 1 : 0;
        m_chunkCount = m_lastFilteredEntityCount >= 2 ? 2 : 0;
        m_chunkRatio = m_lastFilteredEntityCount / 4.f;
        m_cvarWalkOk = false;
        m_bandEnforced = false;
        m_overlayMinPx = 0;
        m_zeroPushRatio = m_lastFilteredEntityCount <= 1 ? 1.f : 0.f;
    }
    m_health.reset_bad_frames();
#endif

    return true;
}

// ══════════════════════════════════════════════════════════════════════
//  post_frame()  —  health evaluation, accept gate, decoy, heartbeat
// ══════════════════════════════════════════════════════════════════════

} // namespace radar::real_adapter
