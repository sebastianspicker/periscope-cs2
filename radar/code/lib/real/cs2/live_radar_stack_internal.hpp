// live_radar_stack_internal.hpp — shared helpers for live_radar_stack*.cpp TUs.
#pragma once

#include "real/cs2/live_radar_stack.hpp"
#include "real/win/api_table.hpp"

#include <cstring>
#include <cstdint>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

namespace real::cs2::stack {
namespace detail {

inline std::uint64_t xorshift(std::uint64_t& s) noexcept {
  s ^= s << 13;
  s ^= s >> 7;
  s ^= s << 17;
  return s;
}

inline bool rd(RemoteReadFn fn, std::uint64_t a, void* b, std::size_t n) {
  return fn && fn(a, b, n);
}

template <typename T>
inline bool rd_t(RemoteReadFn fn, std::uint64_t a, T& o) {
  return rd(fn, a, &o, sizeof(T));
}

#if LR_PLATFORM_WINDOWS
inline bool process_name_running(const char* needle) noexcept {
  auto& api = real::win::g_Api();
  if (!api.resolved || !api.CreateToolhelp32Snapshot) return false;
  HANDLE snap = api.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (!snap || snap == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  bool hit = false;
  if (api.Process32FirstW(snap, &pe)) {
    do {
      char name[MAX_PATH]{};
      WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, name, MAX_PATH, nullptr,
                          nullptr);
      if (std::strstr(name, needle)) {
        hit = true;
        break;
      }
    } while (api.Process32NextW(snap, &pe));
  }
  if (api.CloseHandle) api.CloseHandle(snap);
  else CloseHandle(snap);
  return hit;
}
#endif

}  // namespace detail

using detail::xorshift;
using detail::rd;
using detail::rd_t;
#if LR_PLATFORM_WINDOWS
using detail::process_name_running;
#endif

}  // namespace real::cs2::stack