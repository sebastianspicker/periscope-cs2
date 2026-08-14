#include "real/win/timing.hpp"
#include "real/win/api_table.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS
#include <intrin.h>

namespace real::win {

void fuzzed_sleep(int32_t base_ms, int32_t jitter_pct) noexcept {
  if (base_ms <= 0) return;

  // Simple PRNG seeded once. __rdtscp writes the processor id through
  // its argument — never pass a null pointer (MSVC AV at runtime).
  unsigned int aux = 0;
  static uint64_t s_rng = __rdtscp(&aux);
  s_rng ^= s_rng << 13;
  s_rng ^= s_rng >> 7;
  s_rng ^= s_rng << 17;

  if (jitter_pct < 0) jitter_pct = 0;
  if (jitter_pct > 100) jitter_pct = 100;

  int32_t jitter_range = (jitter_pct * 2 + 1);
  int32_t jitter_off =
      static_cast<int32_t>(s_rng % static_cast<uint64_t>(jitter_range)) - jitter_pct;
  int32_t actual_ms = base_ms * (100 + jitter_off) / 100;
  if (actual_ms < 1) actual_ms = 1;

  LARGE_INTEGER interval;
  interval.QuadPart = -static_cast<int64_t>(actual_ms) * 10000LL;

  auto& api = g_Api();
  if (api.NtDelayExecution) {
    api.NtDelayExecution(FALSE, &interval);
  } else {
    ::Sleep(static_cast<DWORD>(actual_ms));
  }
}

void yield() noexcept {
  auto& api = g_Api();
  if (api.NtDelayExecution) {
    LARGE_INTEGER zero = {};
    api.NtDelayExecution(FALSE, &zero);
  } else {
    ::Sleep(0);
  }
}

}  // namespace real::win

#else
namespace real::win {
void fuzzed_sleep(int32_t, int32_t) noexcept {}
void yield() noexcept {}
}  // namespace real::win
#endif
