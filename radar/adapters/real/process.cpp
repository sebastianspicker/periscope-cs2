// process.cpp — Complete cross-platform process operations.
// Educational framework: TECHNIQUE, SCAR, BLUE, MITIGATION per function.

#include "real/process.hpp"
#include "real/platform.hpp"
#include "real/error.hpp"
#include "real/win/xorstr.hpp"

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

#if LR_PLATFORM_WINDOWS
#  include "real/win/api_table.hpp"
#  include "real/win/windows_h.hpp"
#  include "real/cs2/process_cache.hpp"
#elif LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#  include <dirent.h>
#  include <sys/stat.h>
#elif defined(__APPLE__)
#  include <cerrno>
#  include <csignal>
#  include <libproc.h>
#  include <sys/sysctl.h>
#endif

namespace real {

// ── Internal helpers ───────────────────────────────────────────────

static std::string lower(const std::string& s) {
  std::string r = s;
  for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return r;
}

// ── enum_processes ─────────────────────────────────────────────────
//
// TECHNIQUE: Enumerate running processes via Toolhelp32Snapshot (Windows)
// or /proc filesystem (Linux).
//
// SCAR: Process enumeration is normal OS behavior. Not directly detectable.
//
// BLUE: Anti-cheats also enumerate processes. Cross-reference with handle
// table to find processes holding handles to the game.

Result<std::vector<ProcessInfo>> enum_processes() {
  std::vector<ProcessInfo> processes;

#if LR_PLATFORM_WINDOWS
  {
    // Prefer Toolhelp32 — stable layout, no fragile NtQSI structure parsing.
    auto& api = real::win::g_Api();
    if (api.resolved && api.CreateToolhelp32Snapshot && api.Process32FirstW &&
        api.Process32NextW) {
      HANDLE snap = api.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
      if (snap && snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (api.Process32FirstW(snap, &pe)) {
          do {
            ProcessInfo pi;
            pi.pid = pe.th32ProcessID;
            pi.parent_pid = pe.th32ParentProcessID;
            char name[MAX_PATH]{};
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, name,
                                static_cast<int>(sizeof(name)), nullptr, nullptr);
            pi.name = name;
            processes.push_back(std::move(pi));
          } while (api.Process32NextW(snap, &pe));
        }
        if (api.CloseHandle) api.CloseHandle(snap);
        else ::CloseHandle(snap);
      }
    }

    // Fallback: NtQuerySystemInformation process cache
    if (processes.empty()) {
      const auto& list = real::cs2::get_cached_process_list();
      for (const auto& entry : list) {
        ProcessInfo pi;
        pi.pid = entry.pid;
        pi.parent_pid = entry.parentPid;
        pi.name = entry.name;
        processes.push_back(pi);
      }
    }
  }

#elif LR_PLATFORM_LINUX
  DIR* proc = opendir("/proc");
  if (!proc) return os_error("opendir /proc");

  struct dirent* entry;
  while ((entry = readdir(proc)) != nullptr) {
    // Only numeric directories (process IDs)
    if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;

    uint32_t pid = atoi(entry->d_name);
    if (pid == 0) continue;

    ProcessInfo pi;
    pi.pid = pid;

    // Read process name from /proc/pid/comm
    char comm_path[64];
    snprintf(comm_path, sizeof(comm_path), "/proc/%u/comm", pid);
    FILE* f = fopen(comm_path, "r");
    if (f) {
      char name[256] = {};
      fgets(name, sizeof(name), f);
      size_t len = strlen(name);
      if (len > 0 && name[len-1] == '\n') name[len-1] = '\0';
      pi.name = name;
      fclose(f);
    }

    // Read parent PID from /proc/pid/status
    char status_path[64];
    snprintf(status_path, sizeof(status_path), "/proc/%u/status", pid);
    f = fopen(status_path, "r");
    if (f) {
      char line[256];
      while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "PPid:\t%u", &pi.parent_pid) == 1) break;
      }
      fclose(f);
    }

    processes.push_back(pi);
  }
  closedir(proc);
#endif

  return processes;
}

// ── find_process ───────────────────────────────────────────────────

Result<ProcessInfo> find_process(const std::string& name) {
  auto procs = enum_processes();
  if (!procs) return Result<ProcessInfo>({}, procs.error_msg);

  std::string lower_name = lower(name);
  for (const auto& p : *procs) {
    if (lower(p.name) == lower_name) {
      return p;
    }
  }
  return Result<ProcessInfo>({}, "Process not found: " + name);
}

// ── open_process ───────────────────────────────────────────────────
//
// TECHNIQUE: OpenProcess to get a handle with specified access rights.
//
// SCAR: The handle is VISIBLE in the system handle table.
// EnumProcessHandles can find it. This is the T0 detection vector.
//
// BLUE: Handle enumeration (t0_blue/handle_graph_monitor.cpp).
//
// MITIGATION: Process mitigation policies can block handle opening.
// PPL (Protected Process Light) blocks PROCESS_VM_READ.
//
// WARNING: Do NOT use this function to open CS2 directly.
// All CS2 handles must come through HijackReader (handle duplication
// from a donor process). OpenProcess on CS2 creates an immediately-
// detectable handle in the system handle table.

Result<uint64_t> open_process(uint32_t pid, unsigned long access) {
#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  OBJECT_ATTRIBUTES oa = { sizeof(oa) };
  CLIENT_ID cid = { reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid)), nullptr };
  HANDLE h = nullptr;
  NTSTATUS status = api.NtOpenProcess(&h, access, &oa, &cid);
  if (status < 0) return os_error(OBF("NtOpenProcess"));
#ifndef NDEBUG
  std::printf("[proc] %s: pid=%u access=0x%lx handle=0x%llx\n", OBF("NtOpenProcess"),
              pid, access, (unsigned long long)(uintptr_t)h);
#endif
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(h));
#else
  (void)pid; (void)access;
  return Result<uint64_t>(0, "OpenProcess requires Windows");
#endif
}

// ── close_process ──────────────────────────────────────────────────

Result<void> close_process(uint64_t handle) {
#if LR_PLATFORM_WINDOWS
  if (handle && handle != (uint64_t)-1) {
    CloseHandle(reinterpret_cast<HANDLE>(handle));
  }
  return Result<void>();
#else
  (void)handle;
  return Result<void>();
#endif
}

// ── get_parent_pid ─────────────────────────────────────────────────

Result<uint32_t> get_parent_pid(uint32_t pid) {
#if LR_PLATFORM_LINUX
  char path[64];
  snprintf(path, sizeof(path), "/proc/%u/status", pid);
  FILE* f = fopen(path, "r");
  if (!f) return Result<uint32_t>(0, "open status");
  char line[256];
  uint32_t ppid = 0;
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "PPid:\t%u", &ppid) == 1) break;
  }
  fclose(f);
  return ppid;
#elif LR_PLATFORM_WINDOWS
  {
    auto ppid = real::cs2::get_parent_pid_cached(pid);
    if (ppid) return *ppid;
  }
  return Result<uint32_t>(0, "PID not found");
#elif defined(__APPLE__)
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
  kinfo_proc process{};
  std::size_t process_size = sizeof(process);
  if (::sysctl(mib, sizeof(mib) / sizeof(mib[0]), &process, &process_size, nullptr, 0) != 0 ||
      process_size != sizeof(process)) {
    return Result<uint32_t>(0, "sysctl(KERN_PROC_PID) failed");
  }
  return static_cast<uint32_t>(process.kp_eproc.e_ppid);
#else
  (void)pid;
  return Result<uint32_t>(0, "Process parent lookup not available");
#endif
}

// ── get_process_path ───────────────────────────────────────────────
//
// NOTE: Uses OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION) for path
// queries. For CS2, avoid calling this function if possible — prefer
// querying the path from NtQuerySystemInformation process list instead
// of opening a handle. If unavoidable, this is lower risk than VM_READ
// handles, but still creates a handle-table entry.

Result<std::string> get_process_path(uint32_t pid) {
#if LR_PLATFORM_LINUX
  char path[64];
  snprintf(path, sizeof(path), "/proc/%u/exe", pid);
  char buf[1024] = {};
  ssize_t len = readlink(path, buf, sizeof(buf) - 1);
  if (len < 0) return os_error("readlink exe");
  return std::string(buf);
#elif LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  OBJECT_ATTRIBUTES oa = { sizeof(oa) };
  CLIENT_ID cid = { reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid)), nullptr };
  HANDLE h = nullptr;
  NTSTATUS status = api.NtOpenProcess(&h, PROCESS_QUERY_LIMITED_INFORMATION, &oa, &cid);
  if (status < 0) return Result<std::string>("", "NtOpenProcess failed");
  char path[1024] = {};
  DWORD size = sizeof(path);
  if (!QueryFullProcessImageNameA(h, 0, path, &size)) {
    api.NtClose(h);
    return Result<std::string>("", "QueryFullProcessImageName failed");
  }
  api.NtClose(h);
  return std::string(path);
#elif defined(__APPLE__)
  char path[PROC_PIDPATHINFO_MAXSIZE] = {};
  const int length = ::proc_pidpath(static_cast<int>(pid), path, sizeof(path));
  if (length <= 0) return Result<std::string>("", "proc_pidpath failed");
  return std::string(path, static_cast<std::size_t>(length));
#else
  (void)pid;
  return Result<std::string>("", "Process path not available");
#endif
}

// ── is_process_running ─────────────────────────────────────────────
//
// NOTE: Uses OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION) to check
// if a process is alive. For CS2, prefer is_cs2_alive() which queries
// NtQuerySystemInformation without opening any handle.

Result<bool> is_process_running(uint32_t pid) {
#if LR_PLATFORM_LINUX
  char path[64];
  snprintf(path, sizeof(path), "/proc/%u", pid);
  struct stat st;
  return stat(path, &st) == 0;
#elif LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  OBJECT_ATTRIBUTES oa = { sizeof(oa) };
  CLIENT_ID cid = { reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid)), nullptr };
  HANDLE h = nullptr;
  NTSTATUS status = api.NtOpenProcess(&h, PROCESS_QUERY_LIMITED_INFORMATION, &oa, &cid);
  if (status < 0) return false;
  DWORD exit_code = 0;
  bool running = api.GetExitCodeProcess(h, &exit_code) && exit_code == STILL_ACTIVE;
  api.NtClose(h);
  return running;
#elif defined(__APPLE__)
  if (::kill(static_cast<pid_t>(pid), 0) == 0) return true;
  return errno == EPERM;
#else
  (void)pid;
  return Result<bool>(false, "Process running check not available");
#endif
}

}  // namespace real
