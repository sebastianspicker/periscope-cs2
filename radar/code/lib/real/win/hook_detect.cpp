#include "real/win/hook_detect.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/api_integrity.hpp"
#include "real/win/eat_util.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/windows_h.hpp"
#include "real/win/xorstr.hpp"

#include <cstdint>
#include <cstring>

namespace real::win {
namespace {

struct SnapshotEntry {
  void* address = nullptr;
  uint8_t saved[16]{};
  bool initialized = false;
};

static SnapshotEntry s_snapshots[20]{};
static constexpr size_t kSnapshotCount = sizeof(s_snapshots) / sizeof(s_snapshots[0]);
static bool s_initialized = false;
static uintptr_t s_ntdll_base = 0;

static const char* get_api_name(size_t index) {
  switch (index) {
    case 0: return OBF("NtOpenProcess");
    case 1: return OBF("NtReadVirtualMemory");
    case 2: return OBF("NtWriteVirtualMemory");
    case 3: return OBF("NtClose");
    case 4: return OBF("NtQuerySystemInformation");
    case 5: return OBF("NtDuplicateObject");
    case 6: return OBF("NtCreateThreadEx");
    case 7: return OBF("NtDelayExecution");
    case 8: return OBF("NtSetInformationThread");
    case 9: return OBF("NtQueryInformationProcess");
    case 10: return OBF("NtSuspendProcess");
    case 11: return OBF("NtResumeProcess");
    case 12: return OBF("NtProtectVirtualMemory");
    case 13: return OBF("NtCreateSection");
    case 14: return OBF("NtMapViewOfSection");
    case 15: return OBF("NtQueryVirtualMemory");
    case 16: return OBF("NtDeviceIoControlFile");
    case 17: return OBF("NtTraceEvent");
    case 18: return OBF("EtwEventWrite");
    case 19: return OBF("LdrLoadDll");
    default: return nullptr;
  }
}

static void capture_all() {
  s_ntdll_base = peb::find_module("ntdll");
  if (!s_ntdll_base) return;
  for (size_t i = 0; i < kSnapshotCount; ++i) {
    const char* name = get_api_name(i);
    auto* addr = name ? eat::resolve_export(s_ntdll_base, name) : nullptr;
    if (addr) {
      s_snapshots[i].address = addr;
      std::memcpy(s_snapshots[i].saved, addr, sizeof(SnapshotEntry::saved));
      s_snapshots[i].initialized = true;
    } else {
      s_snapshots[i] = {};
    }
  }
}

}  // anonymous namespace

void init_hook_detection() noexcept {
  if (s_initialized) return;
  capture_all();
  s_initialized = true;
}

void refresh_hook_baseline() noexcept {
  capture_all();
  s_initialized = true;
}

bool check_hooks() noexcept {
  if (!s_initialized) return false;

  for (size_t i = 0; i < kSnapshotCount; ++i) {
    if (!s_snapshots[i].initialized || !s_snapshots[i].address) continue;

    uint8_t current[16];
    std::memcpy(current, s_snapshots[i].address, sizeof(current));
    if (std::memcmp(current, s_snapshots[i].saved, sizeof(current)) != 0) {
      return true;
    }
    // Also classify prologue for hooks that match the baseline if baseline
    // itself was taken while already hooked.
    if (classify_prologue(current, 16) != HookKind::None) {
      // Only flag if baseline was clean
      if (classify_prologue(s_snapshots[i].saved, 16) == HookKind::None) {
        return true;
      }
    }
  }
  return false;
}

HookSnapshotReport hook_detection_report() noexcept {
  HookSnapshotReport rep{};
  rep.ready = s_initialized;
  rep.tracked = static_cast<int>(kSnapshotCount);
  for (size_t i = 0; i < kSnapshotCount; ++i) {
    if (!s_snapshots[i].initialized) continue;
    ++rep.initialized;
    if (!s_snapshots[i].address) continue;
    uint8_t current[16];
    std::memcpy(current, s_snapshots[i].address, sizeof(current));
    if (std::memcmp(current, s_snapshots[i].saved, sizeof(current)) != 0) {
      ++rep.hooked;
    }
  }
  return rep;
}

}  // namespace real::win

#else
namespace real::win {
void init_hook_detection() noexcept {}
bool check_hooks() noexcept { return false; }
HookSnapshotReport hook_detection_report() noexcept { return {}; }
void refresh_hook_baseline() noexcept {}
}  // namespace real::win
#endif
