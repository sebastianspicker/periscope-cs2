// real_backend_test.cpp — Smoke test for real-platform backends.
// Verifies that the platform abstraction layer and backend interfaces
// compile and link correctly. Does NOT execute privileged operations
// (no actual system calls, no hardware access).
//
// Built only when LR_ENABLE_REAL_* options are ON.

#include "real/real_fwd.hpp"
#include "ac/types.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int fails = 0;

#define TEST(cond, msg) do { \
  if (!(cond)) { \
    std::printf("FAIL: %s\n", msg); \
    ++fails; \
  } else { \
    std::printf("ok: %s\n", msg); \
  } \
} while(0)

// ── Platform detection tests ───────────────────────────────────────

static void test_platform_macros() {
  std::printf("\n=== Platform Macros ===\n");

  // At least one platform must be detected
  TEST(LR_PLATFORM_WINDOWS || LR_PLATFORM_LINUX,
       "platform detected (windows or linux)");

  // Architecture must be detected
  TEST(LR_ARCH_X64 || LR_ARCH_X86 || LR_ARCH_ARM64,
       "architecture detected");

  // Only one platform is active
  TEST(LR_PLATFORM_WINDOWS + LR_PLATFORM_LINUX == 1,
       "exactly one platform");

  std::printf("  windows=%d linux=%d x64=%d x86=%d arm64=%d\n",
              LR_PLATFORM_WINDOWS, LR_PLATFORM_LINUX,
              LR_ARCH_X64, LR_ARCH_X86, LR_ARCH_ARM64);
}

// ── Error / Result tests ───────────────────────────────────────────

static void test_error_result() {
  std::printf("\n=== Error / Result ===\n");

  // Result<void> default construction
  real::Result<void> void_ok;
  TEST(void_ok.ok, "Result<void> default ok");

  // Result<void> error construction
  real::Result<void> void_err("something failed");
  TEST(!void_err.ok, "Result<void> error not ok");
  TEST(void_err.error_msg == "something failed",
       "Result<void> error message preserved");

  // Result<int> value construction
  real::Result<int> int_val(42);
  TEST(int_val.ok, "Result<int> value ok");
  TEST(*int_val == 42, "Result<int> dereference");

  // Result<int> error construction
  real::Result<int> int_err(-1, "bad value");
  TEST(!int_err.ok, "Result<int> error not ok");
  TEST(int_err.error_msg == "bad value",
       "Result<int> error message");
}

// ── Memory backend interface tests ─────────────────────────────────

static void test_memory_types() {
  std::printf("\n=== Memory Types ===\n");

  // MappedRegion construction
  real::MappedRegion region;
  TEST(region.base == nullptr, "MappedRegion default base null");
  TEST(region.size == 0, "MappedRegion default size zero");

  // ProcessInfo construction
  real::ProcessInfo pi;
  TEST(pi.pid == 0, "ProcessInfo pid default zero");
  TEST(pi.name.empty(), "ProcessInfo name default empty");

  // LibHandle construction
  real::LibHandle lib;
  TEST(lib.native == nullptr, "LibHandle default null");
}

// ── Socket / C2 client tests ───────────────────────────────────────

static void test_net_types() {
  std::printf("\n=== Networking Types ===\n");

  real::net::Socket sock;
  TEST(sock.fd == -1, "Socket default fd = -1");

  real::net::HttpResponse resp;
  TEST(resp.status_code == 0, "HttpResponse default status 0");
  TEST(resp.body.empty(), "HttpResponse default body empty");

  // C2Message construction
  real::net::C2Message msg;
  TEST(msg.type == real::net::C2MessageType::Heartbeat,
       "C2Message default type Heartbeat");
  TEST(msg.seq == 0, "C2Message default seq 0");
}

// ── GPU / Render types ─────────────────────────────────────────────

static void test_gpu_types() {
  std::printf("\n=== GPU Types ===\n");

  real::gpu::DrawVertex vertex{};
  TEST(vertex.x == 0.0f && vertex.y == 0.0f, "DrawVertex default position 0");
  TEST(vertex.color == 0, "DrawVertex default color 0");

  real::gpu::RadarBlipLayout blip{};
  TEST(blip.health == 0, "RadarBlipLayout default health 0");
  TEST(!blip.is_local, "RadarBlipLayout default is_local false");

  real::gpu::RadarFrame frame;
  TEST(frame.blips.empty(), "RadarFrame default blips empty");
}

// ── DMA types ──────────────────────────────────────────────────────

static void test_dma_types() {
  std::printf("\n=== DMA Types ===\n");
#if defined(LR_HAS_REAL_DMA)
  real::dma::PciAddress addr;
  TEST(addr.bus == 0, "PciAddress bus default 0");
  TEST(addr.device == 0, "PciAddress device default 0");

  real::dma::PciDevice dev;
  TEST(dev.vendor_id == 0, "PciDevice vendor default 0");
  TEST(!dev.is_dma_capable, "PciDevice dma_capable default false");
#else
  TEST(true, "DMA types skipped (LR_ENABLE_REAL_DMA off)");
#endif
}

// ── VMX types ──────────────────────────────────────────────────────

static void test_vmx_types() {
  std::printf("\n=== VMX Types ===\n");
#if defined(LR_HAS_REAL_VMX)
  real::vmx::EptPointer eptp;
  TEST(eptp.value == 0, "EptPointer default 0");
  // Drive shipped unprivileged surfaces (not a re-implementation).
  auto dump = real::vmx::dump_cpuid();
  TEST(!dump.vendor.empty(), "dump_cpuid vendor");
  TEST(real::vmx::get_cpuid_latency() > 0, "cpuid latency calibrated");
  uint64_t e = 0;
  real::vmx::ept_entry_set_phys_addr(e, 0x1000);
  real::vmx::ept_entry_set_access(e, true, true, true);
  real::vmx::ept_entry_set_memory_type(e, 6);
  TEST(real::vmx::ept_entry_get_phys_addr(e) == 0x1000, "ept phys helper");
  TEST(real::vmx::ept_entry_get_memory_type(e) == 6, "ept memtype helper");
  auto fields = real::vmx::compose_basic_vmcs_fields(real::vmx::VmcsSetupConfig{});
  TEST(!fields.empty(), "compose_basic_vmcs_fields non-empty");
#else
  TEST(true, "VMX types skipped (LR_ENABLE_REAL_VMX off)");
#endif
}

// ── SMM types ──────────────────────────────────────────────────────

static void test_smm_types() {
  std::printf("\n=== SMM Types ===\n");

  // Just verify Result types remain usable when SMM is optional.
  real::Result<void> smm_result;
  TEST(smm_result.ok, "SMM Result<void> default ok");
}

// ── IMemoryBackend integration ─────────────────────────────────────

static void test_backend_integration() {
  std::printf("\n=== Backend Integration ===\n");

  // Verify the ac::Tier enum values used by real backends
  TEST(static_cast<int>(ac::Tier::T0_UsermodeRpm) == 0,
       "Tier T0_UsermodeRpm == 0");
  TEST(static_cast<int>(ac::Tier::T1_SyscallSoft) == 1,
       "Tier T1_SyscallSoft == 1");
  TEST(static_cast<int>(ac::Tier::T2_KernelByovd) == 2,
       "Tier T2_KernelByovd == 2");
  TEST(static_cast<int>(ac::Tier::T3_Hypervisor) == 3,
       "Tier T3_Hypervisor == 3");
}

// ── RealDmaBackend basic tests ─────────────────────────────────────

static void test_real_dma_backend() {
  std::printf("\n=== RealDmaBackend ===\n");
#if defined(LR_HAS_REAL_DMA)
  // Construction
  real::dma::RealDmaBackend backend(false);
  TEST(!backend.is_attached(), "DmaBackend not attached initially");
  TEST(backend.tier() == ac::Tier::T0_UsermodeRpm,
       "DmaBackend reports T0 tier");
  TEST(backend.name() == "real_dma", "DmaBackend name is real_dma");

  // attach should fail gracefully (no actual DMA hardware)
  auto status = backend.attach(0);
  TEST(status == ac::Status::Unavailable || backend.is_attached(),
       "DmaBackend attach returns Unavailable or succeeds");

  // Read while detached (or after failed attach) is non-crashing.
  if (!backend.is_attached()) {
    ac::ReadRequest req{0x1000, 32};
    auto rr = backend.read(req);
    TEST(rr.status == ac::Status::Denied,
         "DmaBackend read Denied when not attached");
  } else {
    ac::ReadRequest req{0x1000, 32};
    auto rr = backend.read(req);
    TEST(rr.status == ac::Status::Ok || rr.status == ac::Status::Denied,
         "DmaBackend read Ok/Denied when attached");
  }
  backend.detach();
  TEST(!backend.is_attached(), "DmaBackend detach clears attached");
  backend.detach();  // idempotent
  TEST(!backend.is_attached(), "DmaBackend double-detach safe");

  // Pure page-walk / scatter logic (no hardware).
  {
    // Minimal present 4K mapping: CR3@0x1000, all tables in a heap buffer
    // exercised via dma_unit_test; here just call build_scatter_list which
    // is always available from the shipped library.
    auto descs = real::dma::build_scatter_list(0x1000, 0x3000, 0x1000);
    TEST(descs.size() == 3, "build_scatter_list splits multi-page");
    TEST(descs[0].src_phys == 0x1000 && descs[0].length == 0x1000,
         "build_scatter_list first descriptor");
  }
#else
  TEST(true, "RealDmaBackend skipped (LR_ENABLE_REAL_DMA off)");
#endif
}

// ── Platform API smoke tests ───────────────────────────────────────

static void test_platform_api() {
  std::printf("\n=== Platform API ===\n");

  // OS error formatting (does not need an actual error)
  auto formatted = real::format_os_error(0);
  TEST(!formatted.empty(), "format_os_error(0) produces non-empty");

  // Memory read/write functions exist and are callable
  // (They'll fail gracefully at runtime without privileges)
  auto mem_result = real::read_physical(0, 4096);
  TEST(!mem_result.ok, "read_physical(0) fails gracefully without privileges");
}

// ── real::mode runtime mode + tier probes (shipped APIs) ───────────

static bool string_contains(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

static void test_mode_runtime() {
  std::printf("\n=== real::mode runtime ===\n");

  // Silence set_mode status lines during pure logic checks.
  const bool saved_verbose = real::mode::config().verbose;
  real::mode::config().verbose = false;
  const real::mode::Mode saved_mode = real::mode::config().mode;

  // mode_name labels
  TEST(std::strcmp(real::mode::mode_name(real::mode::Mode::Real), "real") == 0,
       "mode_name(Real) == real");
  TEST(std::strcmp(real::mode::mode_name(real::mode::Mode::Sim), "sim") == 0,
       "mode_name(Sim) == sim");
  TEST(std::strcmp(real::mode::mode_name(real::mode::Mode::Hybrid), "hybrid") == 0,
       "mode_name(Hybrid) == hybrid");

  // parse_mode
  real::mode::Mode parsed = real::mode::Mode::Hybrid;
  TEST(real::mode::parse_mode("real", parsed) && parsed == real::mode::Mode::Real,
       "parse_mode(real)");
  TEST(real::mode::parse_mode("SIM", parsed) && parsed == real::mode::Mode::Sim,
       "parse_mode(SIM) case-insensitive");
  TEST(real::mode::parse_mode("Hybrid", parsed) && parsed == real::mode::Mode::Hybrid,
       "parse_mode(Hybrid)");
  TEST(!real::mode::parse_mode("nope", parsed), "parse_mode rejects unknown");

  // Programmatic set_mode + may_use_real / may_use_sim
  real::mode::set_mode(real::mode::Mode::Real);
  TEST(real::mode::config().mode == real::mode::Mode::Real, "set_mode(Real)");
  TEST(real::mode::config().may_use_real(), "Real: may_use_real");
  TEST(!real::mode::config().may_use_sim(), "Real: not may_use_sim");

  real::mode::set_mode(real::mode::Mode::Sim);
  TEST(real::mode::config().mode == real::mode::Mode::Sim, "set_mode(Sim)");
  TEST(!real::mode::config().may_use_real(), "Sim: not may_use_real");
  TEST(real::mode::config().may_use_sim(), "Sim: may_use_sim");

  real::mode::set_mode(real::mode::Mode::Hybrid);
  TEST(real::mode::config().mode == real::mode::Mode::Hybrid, "set_mode(Hybrid)");
  TEST(real::mode::config().may_use_real(), "Hybrid: may_use_real");
  TEST(real::mode::config().may_use_sim(), "Hybrid: may_use_sim");

  // describe_mode names the active mode; no CS2 required
  real::mode::set_mode(real::mode::Mode::Sim);
  const std::string mode_desc = real::mode::describe_mode();
  TEST(!mode_desc.empty(), "describe_mode non-empty");
  TEST(string_contains(mode_desc, "sim"), "describe_mode includes mode name");
  TEST(string_contains(mode_desc, "Runtime mode"), "describe_mode header");
  std::printf("--- describe_mode ---\n%s", mode_desc.c_str());

  // Restore caller's mode/verbose for later suites.
  real::mode::set_mode(saved_mode);
  real::mode::config().verbose = saved_verbose;
}

static void test_mode_tiers() {
  std::printf("\n=== real::mode tier availability ===\n");

  // Short tier labels used by describe / curriculum
  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T0_UsermodeRpm), "T0") == 0,
       "tier_short_name T0");
  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T1_Syscall), "T1") == 0,
       "tier_short_name T1");
  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T2_BYOVD), "T2") == 0,
       "tier_short_name T2");
  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T3_Hypervisor), "T3") == 0,
       "tier_short_name T3");
  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T4_DMA), "T4") == 0,
       "tier_short_name T4");

  bool avail[5] = {};
  for (int i = 0; i < 5; ++i) {
    const auto tier = static_cast<real::mode::Tier>(i);
    avail[i] = real::mode::tier_available(tier);
    const auto probe = real::mode::probe_tier(tier);
    TEST(probe.tier == tier, "probe_tier tier tag matches");
    TEST(probe.available == avail[i], "probe_tier matches tier_available");
    TEST(!probe.reason.empty(), "probe_tier reason non-empty");
    std::printf("  %s available=%d reason=%s\n",
                real::mode::tier_short_name(tier),
                avail[i] ? 1 : 0, probe.reason.c_str());
  }

  // Mask bits must match per-tier booleans (bit0=T0 … bit4=T4)
  const std::uint8_t mask = real::mode::available_tiers_mask();
  for (int i = 0; i < 5; ++i) {
    const bool bit = (mask & static_cast<std::uint8_t>(1u << i)) != 0;
    char msg[64];
    std::snprintf(msg, sizeof(msg), "mask bit %d matches tier_available(T%d)", i, i);
    TEST(bit == avail[i], msg);
  }
  std::printf("  available_tiers_mask=0x%02X\n", static_cast<unsigned>(mask));

  // highest_available_tier is -1 or max true index
  const int highest = real::mode::highest_available_tier();
  int expected_highest = -1;
  for (int i = 0; i < 5; ++i) {
    if (avail[i]) expected_highest = i;
  }
  TEST(highest == expected_highest, "highest_available_tier consistent with probes");
  TEST(highest >= -1 && highest <= 4, "highest_available_tier in [-1,4]");
  std::printf("  highest_available_tier=%d\n", highest);

  // probe_all_tiers covers Count entries
  const auto all = real::mode::probe_all_tiers();
  TEST(all.size() == static_cast<std::size_t>(real::mode::Tier::Count),
       "probe_all_tiers size == Tier::Count");

  // describe_tier_availability names each short tier label
  const std::string tier_desc = real::mode::describe_tier_availability();
  TEST(!tier_desc.empty(), "describe_tier_availability non-empty");
  TEST(string_contains(tier_desc, "T0"), "describe includes T0");
  TEST(string_contains(tier_desc, "T1"), "describe includes T1");
  TEST(string_contains(tier_desc, "T2"), "describe includes T2");
  TEST(string_contains(tier_desc, "T3"), "describe includes T3");
  TEST(string_contains(tier_desc, "T4"), "describe includes T4");
  TEST(string_contains(tier_desc, "Tier availability"),
       "describe_tier_availability header");
  std::printf("--- describe_tier_availability ---\n%s", tier_desc.c_str());

  // Within a single run, pure aggregation is stable (no random flip)
  const std::uint8_t mask2 = real::mode::available_tiers_mask();
  TEST(mask2 == mask, "available_tiers_mask stable within run");
  TEST(real::mode::highest_available_tier() == highest,
       "highest_available_tier stable within run");
}

// ── Main ───────────────────────────────────────────────────────────

int main() {
  std::printf("=== Real Backend Smoke Test ===\n");
  std::printf("Platform: %s / arch: %s / compiler: %s\n",
#if LR_PLATFORM_WINDOWS
    "windows",
#elif LR_PLATFORM_LINUX
    "linux",
#else
    "unknown",
#endif
#if LR_ARCH_X64
    "x64",
#elif LR_ARCH_X86
    "x86",
#elif LR_ARCH_ARM64
    "arm64",
#else
    "unknown",
#endif
#if LR_COMPILER_MSVC
    "msvc"
#elif LR_COMPILER_GCC
    "gcc"
#elif LR_COMPILER_CLANG
    "clang"
#else
    "unknown"
#endif
  );

  test_platform_macros();
  test_error_result();
  test_memory_types();
  test_net_types();
  test_gpu_types();
  test_dma_types();
  test_vmx_types();
  test_smm_types();
  test_backend_integration();
  test_real_dma_backend();
  test_platform_api();
  test_mode_runtime();
  test_mode_tiers();

  std::printf("\n=== Results: %d failures ===\n", fails);
  return fails;
}
