#pragma once

#include <cstddef>
#include <cstdint>

namespace real::win {

struct HookSnapshotReport {
  int tracked = 0;
  int initialized = 0;
  int hooked = 0;
  bool ready = false;
};

void init_hook_detection() noexcept;
bool check_hooks() noexcept;

/// Detailed report from the current snapshot comparison.
HookSnapshotReport hook_detection_report() noexcept;

/// Re-snapshot current prologues as the new baseline (after intentional unhook).
void refresh_hook_baseline() noexcept;

}  // namespace real::win
