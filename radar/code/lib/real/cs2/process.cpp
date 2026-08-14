#include "real/cs2/process.hpp"

#include "real/win/xorstr.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#include "real/win/timing.hpp"
#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif
#endif

namespace real::cs2 {
namespace {

bool process_name_matches(std::string_view actual, std::string_view expected) {
#if LR_PLATFORM_WINDOWS
  return actual.size() == expected.size() &&
         std::equal(actual.begin(), actual.end(), expected.begin(),
                    [](unsigned char left, unsigned char right) {
                      return std::tolower(left) == std::tolower(right);
                    });
#else
  return actual == expected;
#endif
}

}  // namespace

std::string AttachResult::describe() const {
  if (!attached) {
    return "CS2 not attached: " + error_msg;
  }
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "CS2 attached: pid=%u base=0x%llx image_size=%zu handle=0x%llx",
                pid, static_cast<unsigned long long>(base_address), image_size,
                static_cast<unsigned long long>(handle));
  return buf;
}

Result<real::ProcessInfo> find_cs2_process() {
  auto processes = real::enum_processes();
  if (!processes) return {{}, processes.error_msg};

  const std::string_view expected = real::mode::config().cs2_process_name;
  const auto found = std::find_if(
      (*processes).begin(), (*processes).end(), [expected](const real::ProcessInfo& process) {
        return process_name_matches(process.name, expected);
      });
  if (found == (*processes).end()) {
    return {{}, "CS2 process not found: " + std::string(expected)};
  }
  return Result<real::ProcessInfo>(*found);
}

Result<std::uint64_t> open_cs2_process(std::uint32_t pid, unsigned long access) {
#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  if (!api.resolved || !api.NtOpenProcess) {
    return {{}, "API table not resolved (NtOpenProcess unavailable)"};
  }
  OBJECT_ATTRIBUTES oa{};
  oa.Length = sizeof(oa);
  CLIENT_ID cid{};
  cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid));
  cid.UniqueThread = nullptr;
  HANDLE handle = nullptr;
  NTSTATUS status = api.NtOpenProcess(&handle, static_cast<ACCESS_MASK>(access), &oa, &cid);
  if (status < 0 || handle == nullptr) {
    return {{}, real::os_error(OBF("NtOpenProcess")).error_msg};
  }
  return Result<std::uint64_t>(reinterpret_cast<std::uint64_t>(handle));
#else
  (void)pid;
  (void)access;
  return {{}, "CS2 process access requires Windows"};
#endif
}

Result<std::uint64_t> get_cs2_base_address(std::uint32_t pid,
                                           std::uint64_t process_handle) {
#if LR_PLATFORM_WINDOWS
  (void)pid;
  auto& api = real::win::g_Api();
  if (!api.resolved || !api.EnumProcessModules || !api.GetModuleInformation) {
    return {{}, "API table not resolved (module query unavailable)"};
  }
  HMODULE module = nullptr;
  DWORD needed = 0;
  HANDLE handle = reinterpret_cast<HANDLE>(process_handle);
  if (!api.EnumProcessModules(handle, &module, sizeof(module), &needed) || needed == 0) {
    return {{}, real::os_error(OBF("EnumProcessModules")).error_msg};
  }

  MODULEINFO info{};
  if (!api.GetModuleInformation(handle, module, &info, sizeof(info))) {
    return {{}, real::os_error(OBF("GetModuleInformation")).error_msg};
  }
  return Result<std::uint64_t>(reinterpret_cast<std::uint64_t>(info.lpBaseOfDll));
#else
  (void)pid;
  (void)process_handle;
  return {{}, "CS2 process access requires Windows"};
#endif
}

Result<std::size_t> get_cs2_image_size(std::uint32_t pid,
                                        std::uint64_t process_handle) {
#if LR_PLATFORM_WINDOWS
  (void)pid;
  auto& api = real::win::g_Api();
  if (!api.resolved || !api.EnumProcessModules || !api.GetModuleInformation) {
    return {{}, "API table not resolved (module query unavailable)"};
  }
  HMODULE module = nullptr;
  DWORD needed = 0;
  HANDLE handle = reinterpret_cast<HANDLE>(process_handle);
  if (!api.EnumProcessModules(handle, &module, sizeof(module), &needed) || needed == 0) {
    return {{}, real::os_error(OBF("EnumProcessModules")).error_msg};
  }

  MODULEINFO info{};
  if (!api.GetModuleInformation(handle, module, &info, sizeof(info))) {
    return {{}, real::os_error(OBF("GetModuleInformation")).error_msg};
  }
  return Result<std::size_t>(static_cast<std::size_t>(info.SizeOfImage));
#else
  (void)pid;
  (void)process_handle;
  return {{}, "CS2 process access requires Windows"};
#endif
}

AttachResult attach_to_cs2(int retries) {
  const int attempts = std::max(1, retries);
  AttachResult result;

  for (int attempt = 0; attempt < attempts; ++attempt) {
    auto process = find_cs2_process();
    if (!process) {
      result.error_msg = process.error_msg.c_str();
    } else {
      result.pid = (*process).pid;
      auto handle = open_cs2_process(result.pid);
      if (!handle) {
        result.error_msg = handle.error_msg.c_str();
      } else {
        result.handle = *handle;
        auto base = get_cs2_base_address(result.pid, result.handle);
        if (!base) {
          result.error_msg = base.error_msg.c_str();
        } else {
          auto image_size = get_cs2_image_size(result.pid, result.handle);
          if (image_size) {
            result.attached = true;
            result.base_address = *base;
            result.image_size = *image_size;
            return result;
          }
          result.error_msg = image_size.error_msg.c_str();
        }
        detach_from_cs2(result.handle);
        result.handle = 0;
      }
    }

    if (attempt + 1 < attempts) {
      real::win::fuzzed_sleep(200);  // short retry delay for demos
    }
  }
  return result;
}

void detach_from_cs2(std::uint64_t handle) {
#if LR_PLATFORM_WINDOWS
  if (handle != 0) {
    auto& api = real::win::g_Api();
    if (api.resolved && api.NtClose) {
      api.NtClose(reinterpret_cast<HANDLE>(handle));
    } else {
      ::CloseHandle(reinterpret_cast<HANDLE>(handle));
    }
  }
#else
  (void)handle;
#endif
}

Result<bool> is_cs2_running(std::uint32_t pid) {
#if LR_PLATFORM_WINDOWS
  if (pid == 0) return Result<bool>(false);
  auto& api = real::win::g_Api();
  if (!api.resolved || !api.NtOpenProcess || !api.NtClose) {
    return {{}, "API table not resolved (process query unavailable)"};
  }
  OBJECT_ATTRIBUTES oa{};
  oa.Length = sizeof(oa);
  CLIENT_ID cid{};
  cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid));
  cid.UniqueThread = nullptr;
  HANDLE handle = nullptr;
  NTSTATUS status =
      api.NtOpenProcess(&handle, PROCESS_QUERY_LIMITED_INFORMATION, &oa, &cid);
  if (status < 0 || handle == nullptr) {
    // Process gone or inaccessible — treat as not running.
    return Result<bool>(false);
  }

  // Zero-timeout wait: still signaled => exited; timeout => alive.
  // Never use a null timeout (infinite wait) on a live process handle.
  LARGE_INTEGER zero{};
  zero.QuadPart = 0;
  if (!api.NtWaitForSingleObject) {
    api.NtClose(handle);
    // Open succeeded; without a wait we can only report alive.
    return Result<bool>(true);
  }
  const NTSTATUS ws = api.NtWaitForSingleObject(handle, FALSE, &zero);
  api.NtClose(handle);
  // NTSTATUS STATUS_TIMEOUT = 0x00000102; STATUS_WAIT_0 = 0.
  constexpr NTSTATUS kStatusTimeout = static_cast<NTSTATUS>(0x00000102L);
  constexpr NTSTATUS kStatusWait0 = static_cast<NTSTATUS>(0x00000000L);
  if (ws == kStatusTimeout) return Result<bool>(true);
  if (ws == kStatusWait0) return Result<bool>(false);
  // Ambiguous wait result: open succeeded, so treat as running.
  return Result<bool>(true);
#else
  (void)pid;
  return {{}, "CS2 process access requires Windows"};
#endif
}

Result<std::vector<real::ProcessInfo::Module>> get_cs2_modules(
    std::uint32_t pid, std::uint64_t process_handle) {
#if LR_PLATFORM_WINDOWS
  (void)pid;
  auto& api = real::win::g_Api();
  HANDLE handle = reinterpret_cast<HANDLE>(process_handle);
  DWORD needed = 0;
  if (!api.EnumProcessModulesEx(handle, nullptr, 0, &needed, LIST_MODULES_ALL)) {
    return {{}, real::os_error(OBF("EnumProcessModulesEx")).error_msg};
  }

  std::vector<HMODULE> handles_vec(needed / sizeof(HMODULE));
  if (handles_vec.empty()) return std::vector<real::ProcessInfo::Module>{};
  if (!api.EnumProcessModulesEx(handle, handles_vec.data(),
                                static_cast<DWORD>(handles_vec.size() * sizeof(HMODULE)),
                                &needed, LIST_MODULES_ALL)) {
    return {{}, real::os_error(OBF("EnumProcessModulesEx")).error_msg};
  }

  const std::size_t count = std::min(handles_vec.size(), needed / sizeof(HMODULE));
  std::vector<real::ProcessInfo::Module> modules;
  modules.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    MODULEINFO info{};
    if (!api.GetModuleInformation(handle, handles_vec[index], &info, sizeof(info))) {
      return {{}, real::os_error(OBF("GetModuleInformation")).error_msg};
    }

    char path[32768]{};
    if (api.GetModuleFileNameExA(handle, handles_vec[index], path, sizeof(path)) == 0) {
      return {{}, real::os_error(OBF("GetModuleFileNameExA")).error_msg};
    }
    modules.push_back({path, reinterpret_cast<std::uint64_t>(info.lpBaseOfDll),
                       static_cast<std::size_t>(info.SizeOfImage)});
  }
  return Result<std::vector<real::ProcessInfo::Module>>(std::move(modules));
#else
  (void)pid;
  (void)process_handle;
  return {{}, "CS2 process access requires Windows"};
#endif
}

}  // namespace real::cs2
