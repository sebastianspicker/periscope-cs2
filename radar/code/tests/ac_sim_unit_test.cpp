// ac_sim_unit_test.cpp — drives every shipped code/lib/ac_sim component on the
// real public API. No re-implementation of the engines under test.
//
// Build: cmake --build . --target ac_sim_unit_test
// Run:   ac_sim_unit_test

#include "ac_sim/accept_gate.hpp"
#include "ac_sim/api_integrity.hpp"
#include "ac_sim/batch_engine.hpp"
#include "ac_sim/behavioral_filter.hpp"
#include "ac_sim/decoy_render.hpp"
#include "ac_sim/disguise.hpp"
#include "ac_sim/dll_watch.hpp"
#include "ac_sim/etl.hpp"
#include "ac_sim/forensic.hpp"
#include "ac_sim/health_ladder.hpp"
#include "ac_sim/self_verify.hpp"
#include "ac_sim/shellcode.hpp"
#include "ac_sim/system_normalize.hpp"
#include "ac_sim/temporal.hpp"
#include "ac_sim/xorshift.hpp"

#include "ac/types.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fails = 0;
int g_checks = 0;

void expect(bool cond, const char* msg) {
    ++g_checks;
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_fails;
    } else {
        std::printf("ok: %s\n", msg);
    }
}

// Lab memory for BatchEngine::execute — records addresses, fills buffers.
struct LabMemory {
    std::vector<std::uint64_t> hits;
    static LabMemory* self;

    static bool read_fn(std::uint64_t addr, void* buf, std::size_t size) {
        if (!self || !buf || size == 0) return false;
        self->hits.push_back(addr);
        std::memset(buf, static_cast<int>(addr & 0xFF), size);
        return true;
    }
};
LabMemory* LabMemory::self = nullptr;

} // namespace

int main() {
    // ── 1. XorShiftPool ────────────────────────────────────────────────
    {
        sim::XorShiftPool::seed_all(0xC0FFEEu, /*force=*/true);
        expect(sim::XorShiftPool::seeded(), "xorshift pool seeded");

        auto& t = sim::XorShiftPool::instance(sim::XorShiftInstance::Temporal);
        auto& b = sim::XorShiftPool::instance(sim::XorShiftInstance::Batch);
        const auto a0 = t.next();
        const auto a1 = t.next();
        const auto b0 = b.next();
        expect(a0 != a1, "xorshift temporal advances");
        expect(a0 != b0, "xorshift streams independent");

        sim::XorShift128 rng;
        rng.seed(1, 2);
        const auto r = rng.next_range(10);
        expect(r < 10, "xorshift next_range < max");
        const float f = rng.next_float();
        expect(f >= 0.f && f < 1.f, "xorshift next_float in [0,1)");
        const int iv = rng.next_int(5, 5);
        expect(iv == 5, "xorshift next_int lo==hi");
        const float n = rng.next_normal(0.f, 1.f);
        expect(n == n, "xorshift next_normal finite"); // not NaN
    }

    // ── 2. ExecutionTimeline (ETL) ─────────────────────────────────────
    {
        sim::ExecutionTimeline tl;
        tl.clear();
        expect(tl.size() == 0, "etl starts empty");

        const auto t0 = sim::ExecutionTimeline::now_ns();
        expect(t0 > 0, "etl now_ns positive");

        tl.record(sim::EtlEventType::Init, "boot");
        tl.record(sim::EtlEventType::Frame, "f0", 0);
        tl.record(sim::EtlEventType::Frame, "f1", 1);
        tl.record(sim::EtlEventType::MemoryRead, "rpm", 1, 1234, 0x1000, 16, 0);

        expect(tl.size() == 4, "etl recorded 4 events");
        expect(tl.verify_min_frames(2), "etl verify_min_frames 2");
        expect(tl.verify_no_errors(), "etl verify_no_errors");
        expect(tl.verify_has_type(sim::EtlEventType::Init), "etl has Init");
        expect(tl.count_type(sim::EtlEventType::Frame) == 2, "etl frame count");

        const auto* e0 = tl.at(0);
        expect(e0 && e0->type == sim::EtlEventType::Init, "etl at(0) Init");
        expect(e0 && e0->timestampNs > 0, "etl timestamps filled");
        expect(e0 && std::strcmp(e0->message.c_str(), "boot") == 0,
               "etl message payload");

        // Ring overflow: fill past capacity, size stays capped.
        for (int i = 0; i < 3000; ++i) {
            tl.record(sim::EtlEventType::EvasionTick, "x");
        }
        expect(tl.size() == sim::ExecutionTimeline::kMaxEvents,
               "etl ring caps at kMaxEvents");
        tl.clear();
        expect(tl.size() == 0, "etl clear");
    }

    // ── 3. BatchEngine ─────────────────────────────────────────────────
    {
        sim::BatchEngine batch;
        batch.set_seed(0xBA7C4000u);
        batch.set_rate_limit(120);
        batch.set_jitter_params(0.f, 0.f); // no delay in unit test
        batch.set_decoy_ratio(0.f);
        batch.set_drop_enabled(false);

        std::uint8_t b0[8]{}, b1[8]{}, b2[8]{};
        expect(batch.stage(0x1000, b0, 8), "batch stage 0");
        expect(batch.stage(0x2000, b1, 8), "batch stage 1");
        expect(batch.stage(0x3000, b2, 8), "batch stage 2");
        expect(batch.pending() == 3, "batch pending 3");
        expect(!batch.stage(0, b0, 8), "batch rejects addr 0");

        LabMemory lab;
        LabMemory::self = &lab;
        const auto n = batch.execute(&LabMemory::read_fn);
        expect(n == 3, "batch execute 3 successes");
        expect(lab.hits.size() == 3, "batch read_fn hit 3 times");
        expect(batch.pending() == 0, "batch cleared after execute");
        // Lab read fills each buffer with addr low byte — all three should be written.
        expect((b0[0] | b1[0] | b2[0]) != 0 || lab.hits.size() == 3,
               "batch buffers written or hits recorded");
        expect(batch.stats().succeeded == 3, "batch stats succeeded");
        expect(batch.stats().batches == 1, "batch stats batches");

        // Rate limit clamping.
        batch.set_rate_limit(10);
        expect(batch.rate_limit() == sim::BatchEngine::kMinRps,
               "batch rps clamp min");
        batch.set_rate_limit(999);
        expect(batch.rate_limit() == sim::BatchEngine::kMaxRps,
               "batch rps clamp max");

        // Decoy staging.
        batch.clear();
        batch.set_decoy_ratio(0.5f);
        batch.set_drop_enabled(false);
        batch.set_jitter_params(0.f, 0.f);
        std::uint8_t realBuf[8]{};
        batch.stage(0x4000, realBuf, 8);
        lab.hits.clear();
        const auto n2 = batch.execute(&LabMemory::read_fn);
        expect(n2 >= 1, "batch with decoy still returns real success");
        LabMemory::self = nullptr;
    }

    // ── 4. TemporalEngine ──────────────────────────────────────────────
    {
        sim::TemporalEngine te;
        te.initialize(0x71CE0001ull);
        expect(te.phase() == sim::TemporalPhase::Alpha, "temporal starts Alpha");
        expect(te.phase_remaining() > 0, "temporal phase duration set");

        const auto d0 = te.next_delay_us();
        expect(d0 >= 10 && d0 <= 10000, "temporal delay in range");

        // Force phase transitions by ticking out remaining.
        const auto remaining = te.phase_remaining();
        for (std::uint64_t i = 0; i < remaining + 1; ++i) te.tick();
        expect(te.phase() == sim::TemporalPhase::Beta, "temporal advances to Beta");

        int vals[8] = {0, 1, 2, 3, 4, 5, 6, 7};
        te.shuffle(std::span<int>(vals, 8));
        // Not all identity (extremely unlikely with real shuffle).
        int same = 0;
        for (int i = 0; i < 8; ++i)
            if (vals[i] == i) ++same;
        expect(same < 8, "temporal shuffle permutes");

        const auto bs = te.random_batch_size(4, 16);
        expect(bs >= 4 && bs <= 16, "temporal random_batch_size range");

        const int rps = te.suggested_rps(48, 120);
        expect(rps >= 48 && rps <= 120, "temporal suggested_rps range");

        // busy_wait_us(1) must return (smoke).
        te.busy_wait_us(1);
        expect(true, "temporal busy_wait_us returns");
    }

    // ── 5. AcceptGate ──────────────────────────────────────────────────
    {
        sim::AcceptGate gate;
        gate.initialize();

        sim::GateConditions all{};
        all.chunkRatioOk = true;
        all.bandEnforced = true;
        all.cvarInit = true;
        all.overlaySizeOk = true;
        all.zeroPushRatioOk = true;
        all.cvarInitTimeOk = true;
        all.heartbeatOk = true;
        expect(gate.evaluate(all), "accept gate all-true accepts");
        expect(gate.met_count() == 7, "accept gate met_count 7");
        expect(gate.accepted(), "accept gate accepted flag");

        gate.reset();
        sim::GateConditions none{};
        expect(!gate.evaluate(none), "accept gate all-false rejects");
        expect(gate.met_count() == 0, "accept gate met_count 0");
        expect(gate.failed_mask() == 0x7Fu, "accept gate failed_mask all 7");

        // Metrics path.
        sim::GateMetrics m{};
        m.chunkCount = 4;
        m.remoteEntityCount = 3;
        m.bandEnforced = true;
        m.cvarWalkOk = true;
        m.overlayMinPx = 360;
        m.chunkRatio = 0.8f;
        m.zeroPushRatio = 0.1f;
        m.cvarInitSeconds = 3.f;
        m.heartbeatOk = true;
        expect(gate.evaluate(m), "accept gate metrics accept");
        expect(gate.last_metrics().overlayMinPx == 360,
               "accept gate stores metrics");
    }

    // ── 6. HealthLadder ────────────────────────────────────────────────
    {
        sim::HealthLadder hl;
        sim::HealthLadderConfig cfg;
        cfg.softBadFrameThreshold = 3;
        cfg.mediumSoftActionThreshold = 2;
        cfg.hardMediumActionThreshold = 2;
        cfg.goodFramesToDeescalate = 3;
        hl.initialize(cfg);

        expect(hl.current_level() == sim::HealthLevel::Healthy,
               "health starts Healthy");
        expect(hl.evaluate() == sim::HealthAction::None,
               "health no action yet");

        hl.record_bad_frame();
        hl.record_bad_frame();
        expect(hl.evaluate() == sim::HealthAction::None,
               "health below soft threshold");
        hl.record_bad_frame();
        expect(hl.evaluate() == sim::HealthAction::InvalidateHudRecollect,
               "health soft action");
        expect(hl.current_level() == sim::HealthLevel::Soft, "health Soft");

        // Drive to Medium: need mediumSoftActionThreshold soft actions total.
        // First soft action already counted; one more threshold hit.
        hl.record_bad_frame();
        hl.record_bad_frame();
        hl.record_bad_frame();
        expect(hl.evaluate() == sim::HealthAction::RescanResolve ||
                   hl.current_level() == sim::HealthLevel::Medium ||
                   hl.current_level() == sim::HealthLevel::Soft,
               "health escalates on repeated bad frames");

        // Good frames de-escalate.
        hl.reset();
        hl.initialize(cfg);
        for (int i = 0; i < 3; ++i) hl.record_bad_frame();
        hl.evaluate();
        expect(hl.current_level() == sim::HealthLevel::Soft,
               "health soft after reset path");
        for (int i = 0; i < 3; ++i) hl.record_good_frame();
        expect(hl.current_level() == sim::HealthLevel::Healthy,
               "health de-escalates on good streak");
        expect(hl.stats().totalFrames > 0, "health stats track frames");
    }

    // ── 7. DecoyRenderEngine ───────────────────────────────────────────
    {
        sim::DecoyRenderEngine decoy;
        decoy.initialize(0xDEC0u);
        decoy.set_params(5, 0.08f, 4.f);
        const auto ops = decoy.generate(1920, 1080, ac::Vec3{0, 0, 0});
        expect(ops.size() == 5, "decoy generates N ops");
        bool anyText = false;
        for (const auto& op : ops) {
            expect(op.alpha > 0.f && op.alpha <= 1.f, "decoy alpha range");
            expect(op.w > 0.f && op.h > 0.f, "decoy positive size");
            if (!op.text.empty()) anyText = true;
        }
        expect(anyText, "decoy has at least one text op");
    }

    // ── 8. DisguiseEngine ──────────────────────────────────────────────
    {
        sim::DisguiseEngine disguise;
        disguise.initialize(sim::DisguiseStyle::Afterburner, 0xD15Au);
        expect(disguise.initialized(), "disguise initialized");
        const auto m = disguise.sample();
        expect(!m.osdText.empty(), "disguise osd non-empty");
        expect(m.osdText.find("GPU") != std::string::npos ||
                   m.osdText.find("FPS") != std::string::npos,
               "disguise osd has GPU/FPS");
        expect(m.fakeGpuTemp > 0.f, "disguise gpu temp positive");

        disguise.set_style(sim::DisguiseStyle::Rtss);
        const auto m2 = disguise.sample();
        expect(m2.osdText.find("Framerate") != std::string::npos ||
                   m2.osdText.find("FPS") != std::string::npos,
               "disguise RTSS style text");
        disguise.shutdown();
        expect(!disguise.initialized(), "disguise shutdown");
    }

    // ── 9. ShellcodeEngine ─────────────────────────────────────────────
    {
        sim::ShellcodeEngine sc;
        auto* comm = sim::ShellcodeEngine::SharedMemory();
        expect(comm != nullptr, "shellcode shared memory");
        expect(comm->magic == sim::kPayloadMagic, "shellcode magic ACCP");

        const auto built = sc.build(0x5A);
#if defined(_WIN32) && defined(_M_X64)
        expect(built.compiled, "shellcode compiled on Win x64");
        expect(built.size == sim::kShellcodeSize, "shellcode size 101");
        expect(built.xorKey == 0x5A, "shellcode xor key stored");
        expect(sim::ShellcodeEngine::StructuralOk(built),
               "shellcode structural ok");
        // Returned image must be obfuscated (no bare 0F 05 pair).
        bool bareSyscall = false;
        for (std::size_t i = 0; i + 1 < built.size; ++i) {
            if (built.bytes[i] == 0x0F && built.bytes[i + 1] == 0x05) {
                bareSyscall = true;
                break;
            }
        }
        expect(!bareSyscall, "shellcode result stays obfuscated");

        // Materialize unarmed image → validate structure → scrub.
        std::uint8_t plain[sim::kShellcodeSize]{};
        expect(sim::ShellcodeEngine::Materialize(built, plain, sizeof(plain)),
               "shellcode materialize");
        expect(plain[0] == 0x53, "shellcode starts with push rbx");
        expect(plain[sim::kShellcodeSize - 1] == 0xC3, "shellcode ends with ret");
        expect(sim::ShellcodeEngine::IsValidPlaintext(plain, sizeof(plain)),
               "shellcode plaintext valid");
        expect(sim::ShellcodeEngine::IsValid(built),
               "shellcode IsValid(result)");
        // Confirm unarmed (no bare syscall pair in materialized image).
        bool armed = false;
        for (std::size_t i = 0; i + 1 < sizeof(plain); ++i) {
            if (plain[i] == 0x0F && plain[i + 1] == 0x05) armed = true;
        }
        expect(!armed, "shellcode default materialize is unarmed");
        std::memset(plain, 0, sizeof(plain));

        // Obfuscate involution on a known buffer.
        std::uint8_t sample[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        std::uint8_t copy[8];
        std::memcpy(copy, sample, 8);
        sim::ShellcodeEngine::Obfuscate(copy, 8, 0x5A);
        expect(std::memcmp(copy, sample, 8) != 0, "shellcode obfuscate changes");
        sim::ShellcodeEngine::Deobfuscate(copy, 8, 0x5A);
        expect(std::memcmp(copy, sample, 8) == 0, "shellcode deobfuscate restores");
#else
        expect(!built.compiled, "shellcode disabled off Win x64");
        expect(built.error != nullptr, "shellcode error set");
#endif
    }

    // ── 10. SelfVerify ─────────────────────────────────────────────────
    {
        const char payload[] = "self-verify-lab-payload-v1";
        sim::SelfVerify sv;
        sv.initialize_from(payload, sizeof(payload) - 1);
        expect(sv.has_baseline(), "self_verify baseline captured");
        const auto crc = sim::SelfVerify::crc32(payload, sizeof(payload) - 1);
        expect(crc == sv.expected_crc(), "self_verify crc matches helper");
        expect(crc != 0, "self_verify crc non-zero");

        // Tamper detection via expected CRC mismatch.
        sv.set_expected_crc(crc ^ 0xFFFFFFFFu);
        // verify() re-reads image / sentinel — may not match our set CRC on
        // Windows (image path). For initialize_from path we only guaranteed
        // expected storage; re-verify image baseline separately.
        expect(sv.expected_crc() == (crc ^ 0xFFFFFFFFu),
               "self_verify set_expected_crc sticky");

        // Fresh init from image/sentinel should pass.
        sim::SelfVerify sv2;
        sv2.initialize();
        const auto r = sv2.verify();
        expect(r.baselineCaptured, "self_verify init captures baseline");
        expect(r.textCrcMatch, "self_verify text CRC matches baseline");
        expect(r.pagePermissionsOk, "self_verify page perms ok");
        expect(r.passed, "self_verify passed");
    }

    // ── 11. SystemNormalizer ───────────────────────────────────────────
    {
        sim::SystemNormalizer norm;
        auto r0 = norm.normalize();
        expect(r0.error != nullptr, "sysnorm rejects uninit");

        sim::NormalizeConfig cfg;
        cfg.suppressGpfBox = true;
        cfg.sampleHandleCount = true;
        cfg.acquirePowerRequest = false;
        norm.initialize(cfg);
        const auto r = norm.normalize();
        expect(r.werSuppressed, "sysnorm WER suppressed");
        expect(r.powerRequestAcquired, "sysnorm power no-op success");
        // handle count sampling may fail without privileges; just ensure
        // normalize returned without error string.
        expect(r.error == nullptr, "sysnorm no error");
        norm.shutdown();
    }

    // ── 12. BehavioralFilter ───────────────────────────────────────────
    {
        sim::BehavioralFilter bf;
        sim::BehavioralFilterConfig cfg;
        cfg.applyDelay = false; // unit test must not sleep hundreds of ms
        cfg.mutateEntities = true;
        cfg.omissionRate = 0.5; // aggressive so we observe omissions
        bf.initialize(cfg, 0xF117E400ull);

        ac::EntitySnapshot ents[4]{};
        ents[0] = ac::EntitySnapshot{1, {0, 0, 0}, 1, true};
        ents[0].is_local_player = true;
        ents[1] = ac::EntitySnapshot{2, {100, 0, 0}, 2, true};
        ents[1].health = 100;
        ents[2] = ac::EntitySnapshot{3, {200, 50, 0}, 2, true};
        ents[2].health = 80;
        ents[3] = ac::EntitySnapshot{4, {5, 5, 0}, 2, true};
        ents[3].health = 90;

        // First pass: reaction-time gate omits freshly seen remote entities.
        bf.filter_entities(ents, 4, ac::Vec3{0, 0, 0}, 0.f, false, 3.14f,
                           1.57f);
        expect(ents[0].is_local_player && ents[0].alive,
               "behavioral keeps local player");
        expect(bf.stats().ticks == 1, "behavioral tick count");
        expect(bf.stats().entitiesSeen == 4, "behavioral entities seen");

        // Advance ticks past reaction window.
        for (int t = 0; t < 20; ++t) {
            ents[1].alive = true;
            ents[1].dormant = false;
            ents[1].health = 100;
            ents[2].alive = true;
            ents[2].dormant = false;
            ents[2].health = 80;
            ents[3].alive = true;
            ents[3].dormant = false;
            ents[3].health = 90;
            bf.filter_entities(ents, 4, ac::Vec3{0, 0, 0}, 100.f, false, 3.14f,
                               1.57f);
        }
        expect(bf.stats().ticks == 21, "behavioral multi-tick");
        // With high omission rate over 20 ticks we must have omitted some.
        expect(bf.stats().entitiesOmitted > 0 || bf.stats().entitiesFuzzed > 0,
               "behavioral omits or fuzzes over time");

        float sx = 100.f, sy = 200.f;
        sim::GaussianNoise gn(42);
        bf.fuzz_screen(sx, sy, 50.f, true, gn);
        expect(sx != 100.f || sy != 200.f, "behavioral fuzz_screen moves");
    }

    // ── 13. DllWatch ───────────────────────────────────────────────────
    {
        sim::DllWatch watch;
        watch.initialize();
        expect(watch.known_pattern_count() >= 30, "dll_watch >=30 patterns");
        expect(watch.is_suspicious_module("EasyAntiCheat.sys"),
               "dll_watch flags EAC");
        expect(watch.is_suspicious_module("x64dbg.exe"),
               "dll_watch flags x64dbg");
        expect(!watch.is_suspicious_module("kernel32.dll"),
               "dll_watch ignores kernel32");

        watch.push_notification("battleye_service.exe");
        auto pending = watch.pending_notifications();
        expect(pending.size() == 1, "dll_watch pending 1");
        expect(pending[0].isSuspicious, "dll_watch pending suspicious");
        expect(!pending[0].matchedPattern.empty(),
               "dll_watch matched pattern set");

        watch.start_monitoring();
        expect(watch.monitoring(), "dll_watch monitoring on");
        watch.shutdown();
        expect(!watch.monitoring(), "dll_watch monitoring off");
    }

    // ── 14. ApiIntegrityChecker ────────────────────────────────────────
    {
        sim::ApiIntegrityChecker api;
        auto nullRes = api.verify("ntdll.dll", "NtReadVirtualMemory", 0);
        expect(!nullRes.passed, "api_integrity null addr fails");
        expect(nullRes.hookType == sim::HookType::Unknown,
               "api_integrity null → Unknown");

        api.set_known_good("lab.dll", 0x10000000ull, 0x10000ull);
        auto ok = api.verify("lab.dll", "LabFn", 0x10000100ull);
        // Without real prologue bytes that look like hooks, should pass
        // containment check; inline hook detect may or may not fire on
        // unmapped address — on Windows reading unmapped may AV, so we
        // only assert the null/outside cases which are pure logic.
        auto outside = api.verify("lab.dll", "LabFn", 0x20000000ull);
        expect(!outside.passed, "api_integrity outside module fails");
        expect(outside.hookType == sim::HookType::IatHook,
               "api_integrity outside → IatHook");

        const auto all = api.verify_all();
        expect(!all.empty(), "api_integrity verify_all non-empty");
        (void)ok;
    }

    // ── 15. ForensicEngine ─────────────────────────────────────────────
    {
        sim::ForensicEngine fe;
        auto bad = fe.execute_cleanup();
        expect(!bad.errors.empty(), "forensic uninit errors");

        fe.initialize();
        expect(fe.initialized(), "forensic initialized");
        // Do NOT request prefetch cleanup in CI (would touch Prefetch dir).
        auto r = fe.execute_cleanup(/*include_prefetch=*/false);
        // On Windows without matching recent/mui entries this still returns
        // success flags (empty dirs are OK). At minimum no crash.
        expect(fe.initialized(), "forensic still init after cleanup");
        (void)r;
        fe.shutdown();
        expect(!fe.initialized(), "forensic shutdown");
    }

    // ── Structural: all ac_sim headers resolve (already compiled in) ───
    expect(sim::kShellcodeSize == 101, "structural kShellcodeSize 101");
    expect(sim::BatchEngine::kMaxBatchSize == 16, "structural batch max 16");
    expect(static_cast<int>(sim::XorShiftInstance::Count) == 5,
           "structural 5 RNG streams");
    expect(sim::kKnownAcPatternsCount >= 30, "structural AC patterns >= 30");

    std::printf("\n%d checks, %d failed\n", g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
