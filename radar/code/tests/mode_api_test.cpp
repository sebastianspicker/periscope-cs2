// mode_api_test.cpp — Focused tests for shipped real::mode APIs.
// Links ac_real_platform (+ ac_common); does not require optional DMA/kernel/net.
//
// Drives the real entry points: set_mode, may_use_*, tier_available,
// available_tiers_mask, highest_available_tier, describe_*, probe_*.

#include "real/mode/mode.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>

static int fails = 0;

#define TEST(cond, msg)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL: %s\n", msg);                                          \
      ++fails;                                                                 \
    } else {                                                                   \
      std::printf("ok: %s\n", msg);                                            \
    }                                                                          \
  } while (0)

static bool contains(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

static void test_mode_control() {
  std::printf("\n=== Mode control ===\n");

  const bool saved_verbose = real::mode::config().verbose;
  const real::mode::Mode saved_mode = real::mode::config().mode;
  real::mode::config().verbose = false;

  TEST(std::strcmp(real::mode::mode_name(real::mode::Mode::Real), "real") == 0,
       "mode_name(Real)");
  TEST(std::strcmp(real::mode::mode_name(real::mode::Mode::Sim), "sim") == 0,
       "mode_name(Sim)");
  TEST(std::strcmp(real::mode::mode_name(real::mode::Mode::Hybrid), "hybrid") == 0,
       "mode_name(Hybrid)");

  real::mode::Mode parsed = real::mode::Mode::Hybrid;
  TEST(real::mode::parse_mode("real", parsed) &&
           parsed == real::mode::Mode::Real,
       "parse_mode(real)");
  TEST(real::mode::parse_mode("SIM", parsed) && parsed == real::mode::Mode::Sim,
       "parse_mode(SIM)");
  TEST(real::mode::parse_mode("Hybrid", parsed) &&
           parsed == real::mode::Mode::Hybrid,
       "parse_mode(Hybrid)");
  TEST(!real::mode::parse_mode("bogus", parsed), "parse_mode rejects unknown");

  real::mode::set_mode(real::mode::Mode::Real);
  TEST(real::mode::config().mode == real::mode::Mode::Real, "set_mode(Real)");
  TEST(real::mode::config().may_use_real(), "Real may_use_real");
  TEST(!real::mode::config().may_use_sim(), "Real not may_use_sim");

  real::mode::set_mode(real::mode::Mode::Sim);
  TEST(real::mode::config().mode == real::mode::Mode::Sim, "set_mode(Sim)");
  TEST(!real::mode::config().may_use_real(), "Sim not may_use_real");
  TEST(real::mode::config().may_use_sim(), "Sim may_use_sim");

  real::mode::set_mode(real::mode::Mode::Hybrid);
  TEST(real::mode::config().mode == real::mode::Mode::Hybrid, "set_mode(Hybrid)");
  TEST(real::mode::config().may_use_real(), "Hybrid may_use_real");
  TEST(real::mode::config().may_use_sim(), "Hybrid may_use_sim");

  real::mode::set_mode(real::mode::Mode::Sim);
  const std::string desc = real::mode::describe_mode();
  TEST(!desc.empty(), "describe_mode non-empty");
  TEST(contains(desc, "sim"), "describe_mode includes mode name");
  TEST(contains(desc, "Runtime mode"), "describe_mode header");
  std::printf("--- describe_mode ---\n%s", desc.c_str());

  // Env reload path: load_from_env must be callable (no crash).
  real::mode::config().load_from_env();
  TEST(true, "load_from_env callable");

  real::mode::set_mode(saved_mode);
  real::mode::config().verbose = saved_verbose;
}

static void test_tier_probes() {
  std::printf("\n=== Tier probes ===\n");

  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T0_UsermodeRpm),
                   "T0") == 0,
       "tier_short_name T0");
  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T1_Syscall),
                   "T1") == 0,
       "tier_short_name T1");
  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T2_BYOVD),
                   "T2") == 0,
       "tier_short_name T2");
  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T3_Hypervisor),
                   "T3") == 0,
       "tier_short_name T3");
  TEST(std::strcmp(real::mode::tier_short_name(real::mode::Tier::T4_DMA),
                   "T4") == 0,
       "tier_short_name T4");

  bool avail[5] = {};
  for (int i = 0; i < 5; ++i) {
    const auto tier = static_cast<real::mode::Tier>(i);
    avail[i] = real::mode::tier_available(tier);
    const auto probe = real::mode::probe_tier(tier);
    TEST(static_cast<int>(probe.tier) == i, "probe_tier tag");
    TEST(probe.available == avail[i], "probe_tier matches tier_available");
    TEST(!probe.reason.empty(), "probe_tier reason non-empty");
    std::printf("  %s available=%d reason=%s\n",
                real::mode::tier_short_name(tier), avail[i] ? 1 : 0,
                probe.reason.c_str());
  }

  const std::uint8_t mask = real::mode::available_tiers_mask();
  for (int i = 0; i < 5; ++i) {
    const bool bit = (mask & static_cast<std::uint8_t>(1u << i)) != 0;
    char msg[72];
    std::snprintf(msg, sizeof(msg),
                  "mask bit %d matches tier_available(T%d)", i, i);
    TEST(bit == avail[i], msg);
  }
  std::printf("  available_tiers_mask=0x%02X\n", static_cast<unsigned>(mask));

  int expected_highest = -1;
  for (int i = 0; i < 5; ++i) {
    if (avail[i]) expected_highest = i;
  }
  const int highest = real::mode::highest_available_tier();
  TEST(highest == expected_highest, "highest_available_tier consistent");
  TEST(highest >= -1 && highest <= 4, "highest_available_tier range");
  std::printf("  highest_available_tier=%d\n", highest);

  const auto all = real::mode::probe_all_tiers();
  TEST(all.size() == static_cast<std::size_t>(real::mode::Tier::Count),
       "probe_all_tiers size");

  const std::string tier_desc = real::mode::describe_tier_availability();
  TEST(!tier_desc.empty(), "describe_tier_availability non-empty");
  TEST(contains(tier_desc, "T0"), "describe includes T0");
  TEST(contains(tier_desc, "T1"), "describe includes T1");
  TEST(contains(tier_desc, "T2"), "describe includes T2");
  TEST(contains(tier_desc, "T3"), "describe includes T3");
  TEST(contains(tier_desc, "T4"), "describe includes T4");
  TEST(contains(tier_desc, "Tier availability"), "describe header");
  std::printf("--- describe_tier_availability ---\n%s", tier_desc.c_str());

  // Stability within a run for pure aggregation
  TEST(real::mode::available_tiers_mask() == mask,
       "mask stable within run");
  TEST(real::mode::highest_available_tier() == highest,
       "highest stable within run");

  // cs2_available is callable without requiring CS2 present
  (void)real::mode::cs2_available();
  TEST(true, "cs2_available callable without CS2");
}

int main() {
  std::printf("=== mode_api_test (shipped real::mode) ===\n");
  std::printf("platform windows=%d linux=%d x64=%d\n", LR_PLATFORM_WINDOWS,
              LR_PLATFORM_LINUX, LR_ARCH_X64);

  test_mode_control();
  test_tier_probes();

  std::printf("\n=== Results: %d failures ===\n", fails);
  return fails;
}
