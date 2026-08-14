#pragma once
#include <cstdint>

namespace real::win {

// Single centralized delay function for the entire project.
// All timing goes through here — NO direct NtDelayExecution calls elsewhere.
// base_ms: base delay in milliseconds
// jitter_pct: ±percentage jitter (default ±20%)
void fuzzed_sleep(int32_t base_ms, int32_t jitter_pct = 20) noexcept;

// Yield remainder of time slice (equivalent to Sleep(0))
void yield() noexcept;

} // namespace real::win
