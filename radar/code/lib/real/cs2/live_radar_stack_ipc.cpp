// Split from live_radar_stack.cpp — see MONOLITH_REFACTOR_LEDGER.
#include "real/cs2/live_radar_stack.hpp"
#include "real/cs2/live_radar_stack_internal.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include "real/cs2/process.hpp"
#include "real/win/api_table.hpp"
#include "real/win/timing.hpp"
#include "real/win/xorstr.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

namespace real::cs2::stack {

#if LR_PLATFORM_WINDOWS
bool DonorIpcClient::connect() noexcept {
  disconnect();
  HANDLE map = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, kDonorMapName);
  if (!map) return false;
  void* view = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(DonorIpcHeader) + kDonorMaxPayload);
  if (!view) {
    CloseHandle(map);
    return false;
  }
  auto* hdr = reinterpret_cast<DonorIpcHeader*>(view);
  if (hdr->magic != kDonorMagic || hdr->version < 1 || hdr->cs2_pid == 0) {
    UnmapViewOfFile(view);
    CloseHandle(map);
    return false;
  }
  HANDLE er = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, kDonorEventReq);
  HANDLE es = OpenEventA(SYNCHRONIZE, FALSE, kDonorEventRsp);
  if (!er || !es) {
    if (er) CloseHandle(er);
    if (es) CloseHandle(es);
    UnmapViewOfFile(view);
    CloseHandle(map);
    return false;
  }
  map_handle_ = map;
  mapped_ = view;
  evt_req_ = er;
  evt_rsp_ = es;
  return true;
}

void DonorIpcClient::disconnect() noexcept {
  if (mapped_) {
    UnmapViewOfFile(mapped_);
    mapped_ = nullptr;
  }
  if (map_handle_) {
    CloseHandle(static_cast<HANDLE>(map_handle_));
    map_handle_ = nullptr;
  }
  if (evt_req_) {
    CloseHandle(static_cast<HANDLE>(evt_req_));
    evt_req_ = nullptr;
  }
  if (evt_rsp_) {
    CloseHandle(static_cast<HANDLE>(evt_rsp_));
    evt_rsp_ = nullptr;
  }
}

std::uint32_t DonorIpcClient::worker_pid() const noexcept {
  return mapped_ ? reinterpret_cast<const DonorIpcHeader*>(mapped_)->worker_pid : 0;
}
std::uint32_t DonorIpcClient::cs2_pid() const noexcept {
  return mapped_ ? reinterpret_cast<const DonorIpcHeader*>(mapped_)->cs2_pid : 0;
}

DonorModules DonorIpcClient::modules() const noexcept {
  DonorModules m{};
  if (!mapped_) return m;
  auto* h = reinterpret_cast<const DonorIpcHeader*>(mapped_);
  if (h->version < 2) return m;
  m.client_base = h->client_base;
  m.engine2_base = h->engine2_base;
  m.tier0_base = h->tier0_base;
  m.cs2_base = h->cs2_base;
  m.client_size = h->client_size;
  m.engine2_size = h->engine2_size;
  m.tier0_size = h->tier0_size;
  m.cs2_image_size = h->cs2_image_size;
  m.ready = (h->flags & kDonorFlagModulesReady) != 0 && m.client_base != 0;
  return m;
}

bool DonorIpcClient::wait_modules(int timeout_ms) noexcept {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (modules().ready) return true;
    Sleep(50);
  }
  return modules().ready;
}

bool DonorIpcClient::read(std::uint64_t addr, void* buf, std::size_t size) noexcept {
  if (!mapped_ || !buf || size == 0 || size > kDonorMaxPayload) return false;
  auto* hdr = reinterpret_cast<DonorIpcHeader*>(mapped_);
  for (int spin = 0; spin < 200 && hdr->status == 1; ++spin) Sleep(1);
  hdr->req_addr = addr;
  hdr->req_size = static_cast<std::uint32_t>(size);
  hdr->resp_size = 0;
  hdr->status = 1;
  ++hdr->ticket;
  SetEvent(static_cast<HANDLE>(evt_req_));
  if (WaitForSingleObject(static_cast<HANDLE>(evt_rsp_), 80) != WAIT_OBJECT_0) {
    hdr->status = 0;
    return false;
  }
  if (hdr->status != 2 || hdr->resp_size != size) {
    hdr->status = 0;
    return false;
  }
  std::memcpy(buf, reinterpret_cast<std::uint8_t*>(mapped_) + sizeof(DonorIpcHeader),
              size);
  hdr->status = 0;
  return true;
}

std::uint32_t spawn_donor_worker() noexcept {
  char path[MAX_PATH]{};
  if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) return 0;
  std::string p(path);
  const auto slash = p.find_last_of("\\/");
  std::string dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
  // Disguise: spawn as MpsSvcHost.exe copy if present, else donor_worker.exe
  std::string worker = dir + "\\donor_worker.exe";
  std::string disguise = dir + "\\MpsSvcHost.exe";
  // Best-effort copy for disguise (ignore failure)
  CopyFileA(worker.c_str(), disguise.c_str(), FALSE);
  const char* exe = GetFileAttributesA(disguise.c_str()) != INVALID_FILE_ATTRIBUTES
                       ? disguise.c_str()
                       : worker.c_str();
  STARTUPINFOA si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::string cmd = std::string("\"") + exe + "\"";
  std::vector<char> mutable_cmd(cmd.begin(), cmd.end());
  mutable_cmd.push_back('\0');
  if (!CreateProcessA(exe, mutable_cmd.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, dir.c_str(), &si, &pi)) {
    return 0;
  }
  const DWORD pid = pi.dwProcessId;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  Sleep(250);
  return static_cast<std::uint32_t>(pid);
}

void forensic_cleanup_ipc() noexcept {
  // Best-effort: open and zero mapping, then close events.
  HANDLE map = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, kDonorMapName);
  if (map) {
    void* view = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0,
                               sizeof(DonorIpcHeader) + kDonorMaxPayload);
    if (view) {
      SecureZeroMemory(view, sizeof(DonorIpcHeader) + kDonorMaxPayload);
      UnmapViewOfFile(view);
    }
    CloseHandle(map);
  }
  // Events auto-destroy when last handle closes; try open+close.
  if (HANDLE e = OpenEventA(EVENT_MODIFY_STATE, FALSE, kDonorEventReq)) CloseHandle(e);
  if (HANDLE e = OpenEventA(EVENT_MODIFY_STATE, FALSE, kDonorEventRsp)) CloseHandle(e);
}

void apply_process_disguise(const char* role) noexcept {
  // Educational: console/window title only (no PEB rewrite).
  char title[128]{};
  std::snprintf(title, sizeof(title), "MpsSvc_%s_%u", role ? role : "svc",
                GetCurrentProcessId());
  SetConsoleTitleA(title);
}

#else
bool DonorIpcClient::connect() noexcept { return false; }
void DonorIpcClient::disconnect() noexcept {}
bool DonorIpcClient::read(std::uint64_t, void*, std::size_t) noexcept { return false; }
std::uint32_t DonorIpcClient::worker_pid() const noexcept { return 0; }
std::uint32_t DonorIpcClient::cs2_pid() const noexcept { return 0; }
DonorModules DonorIpcClient::modules() const noexcept { return {}; }
bool DonorIpcClient::wait_modules(int) noexcept { return false; }
std::uint32_t spawn_donor_worker() noexcept { return 0; }
void forensic_cleanup_ipc() noexcept {}
void apply_process_disguise(const char*) noexcept {}
#endif

}  // namespace real::cs2::stack

