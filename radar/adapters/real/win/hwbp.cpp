#include "real/win/hwbp.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS && (LR_ARCH_X64 || LR_ARCH_X86)

#include "real/win/windows_h.hpp"

namespace real::win {

HwbpState hwbp_read() noexcept {
  HwbpState s{};
  CONTEXT ctx{};
  ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
  if (!::GetThreadContext(::GetCurrentThread(), &ctx)) return s;
#if LR_ARCH_X64
  s.dr0 = ctx.Dr0;
  s.dr1 = ctx.Dr1;
  s.dr2 = ctx.Dr2;
  s.dr3 = ctx.Dr3;
  s.dr6 = ctx.Dr6;
  s.dr7 = ctx.Dr7;
#else
  s.dr0 = ctx.Dr0;
  s.dr1 = ctx.Dr1;
  s.dr2 = ctx.Dr2;
  s.dr3 = ctx.Dr3;
  s.dr6 = ctx.Dr6;
  s.dr7 = ctx.Dr7;
#endif
  // Local enable bits in DR7: bits 0,2,4,6
  const bool enabled = (s.dr7 & 0xFF) != 0;
  s.any_active = enabled || s.dr0 || s.dr1 || s.dr2 || s.dr3;
  return s;
}

bool hwbp_detected() noexcept { return hwbp_read().any_active; }

bool hwbp_clear() noexcept {
  CONTEXT ctx{};
  ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
  if (!::GetThreadContext(::GetCurrentThread(), &ctx)) return false;
  ctx.Dr0 = 0;
  ctx.Dr1 = 0;
  ctx.Dr2 = 0;
  ctx.Dr3 = 0;
  ctx.Dr6 = 0;
  ctx.Dr7 = 0;
  return ::SetThreadContext(::GetCurrentThread(), &ctx) != FALSE;
}

}  // namespace real::win

#else

namespace real::win {
HwbpState hwbp_read() noexcept { return {}; }
bool hwbp_detected() noexcept { return false; }
bool hwbp_clear() noexcept { return false; }
}  // namespace real::win

#endif
