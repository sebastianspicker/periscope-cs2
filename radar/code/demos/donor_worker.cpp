/// donor_worker.cpp — Lab L2 pure donor (helper_ticket_protocol).
/// Holds CS2 OpenProcess VM_READ; publishes module bases so UI never opens CS2.
/// Disguise: optional MpsSvcHost title. Forensic: END wipes IPC payload.

#include "real/real_fwd.hpp"
#include "real/win/api_table.hpp"
#include "real/cs2/live_radar_stack.hpp"
#include "real/cs2/process.hpp"
#include "real/cs2/offsets.hpp"

#include <cstdio>
#include <cstring>
#include <cstdint>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

using real::cs2::stack::DonorIpcHeader;
using real::cs2::stack::kDonorMagic;
using real::cs2::stack::kDonorVersion;
using real::cs2::stack::kDonorFlagModulesReady;
using real::cs2::stack::kDonorMaxPayload;
using real::cs2::stack::kDonorMapName;
using real::cs2::stack::kDonorEventReq;
using real::cs2::stack::kDonorEventRsp;

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::printf("=== LR Donor Worker v2 (pure L2 modules + RPM) ===\n");

#if !LR_PLATFORM_WINDOWS
  std::printf("Windows only\n");
  return 1;
#else
  real::cs2::stack::apply_process_disguise("worker");

  auto& api = real::win::g_Api();
  if (!api.resolved) {
    std::printf("FAIL: API table\n");
    return 1;
  }

  auto att = real::cs2::attach_to_cs2();
  if (!att.attached) {
    std::printf("FAIL: %s\n", att.error_msg.c_str());
    return 1;
  }
  std::printf("CS2 pid=%u handle=0x%llx (SCAR on THIS process only)\n", att.pid,
              (unsigned long long)att.handle);

  // Module discovery — UI will consume these without OpenProcess.
  std::uint64_t client = 0, eng = 0, tier0 = 0;
  std::size_t csz = 0, esz = 0, tsz = 0;
  real::cs2::find_client_module(att.pid, att.handle, client, csz);
  real::cs2::find_module_by_basename(att.pid, att.handle, "engine2.dll", eng, esz);
  real::cs2::find_module_by_basename(att.pid, att.handle, "tier0.dll", tier0, tsz);
  std::printf("modules client=0x%llx engine2=0x%llx tier0=0x%llx\n",
              (unsigned long long)client, (unsigned long long)eng,
              (unsigned long long)tier0);

  const size_t map_bytes = sizeof(DonorIpcHeader) + kDonorMaxPayload;
  HANDLE map = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                  static_cast<DWORD>(map_bytes), kDonorMapName);
  if (!map) {
    std::printf("FAIL: CreateFileMapping\n");
    real::cs2::detach_from_cs2(att.handle);
    return 1;
  }
  void* view = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, map_bytes);
  if (!view) {
    CloseHandle(map);
    real::cs2::detach_from_cs2(att.handle);
    return 1;
  }
  std::memset(view, 0, map_bytes);
  auto* hdr = reinterpret_cast<DonorIpcHeader*>(view);
  hdr->magic = kDonorMagic;
  hdr->version = kDonorVersion;
  hdr->cs2_pid = att.pid;
  hdr->worker_pid = GetCurrentProcessId();
  hdr->status = 0;
  hdr->cs2_base = att.base_address;
  hdr->cs2_image_size = static_cast<std::uint32_t>(att.image_size);
  hdr->client_base = client;
  hdr->client_size = static_cast<std::uint32_t>(csz);
  hdr->engine2_base = eng;
  hdr->engine2_size = static_cast<std::uint32_t>(esz);
  hdr->tier0_base = tier0;
  hdr->tier0_size = static_cast<std::uint32_t>(tsz);
  if (client) hdr->flags |= kDonorFlagModulesReady;

  HANDLE evt_req = CreateEventA(nullptr, FALSE, FALSE, kDonorEventReq);
  HANDLE evt_rsp = CreateEventA(nullptr, FALSE, FALSE, kDonorEventRsp);
  if (!evt_req || !evt_rsp) {
    UnmapViewOfFile(view);
    CloseHandle(map);
    real::cs2::detach_from_cs2(att.handle);
    return 1;
  }

  std::printf("IPC ready map=%s worker=%u modules_ready=%d\n", kDonorMapName,
              hdr->worker_pid, (hdr->flags & kDonorFlagModulesReady) ? 1 : 0);
  std::printf("Serving until CS2 exit or END...\n");

  HANDLE hCs2 = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(att.handle));
  std::uint64_t served = 0;

  while (true) {
    if (GetAsyncKeyState(VK_END) & 0x8000) break;
    DWORD code = 0;
    if (GetExitCodeProcess(hCs2, &code) && code != STILL_ACTIVE) break;

    const DWORD w = WaitForSingleObject(evt_req, 200);
    if (w != WAIT_OBJECT_0) continue;
    if (hdr->status != 1) continue;
    const std::uint32_t sz = hdr->req_size;
    if (sz == 0 || sz > kDonorMaxPayload) {
      hdr->status = 3;
      SetEvent(evt_rsp);
      continue;
    }
    SIZE_T br = 0;
    BOOL ok = FALSE;
    void* dst = reinterpret_cast<std::uint8_t*>(view) + sizeof(DonorIpcHeader);
    if (api.resolved && api.ReadProcessMemory)
      ok = api.ReadProcessMemory(hCs2, reinterpret_cast<LPCVOID>(hdr->req_addr), dst,
                                 sz, &br);
    else
      ok = ReadProcessMemory(hCs2, reinterpret_cast<LPCVOID>(hdr->req_addr), dst, sz,
                             &br);
    if (ok && br == sz) {
      hdr->resp_size = sz;
      hdr->status = 2;
      ++served;
      ++hdr->seq;
    } else {
      hdr->resp_size = 0;
      hdr->status = 3;
    }
    SetEvent(evt_rsp);
  }

  std::printf("Forensic wipe + shutdown. served=%llu\n", (unsigned long long)served);
  SecureZeroMemory(view, map_bytes);
  CloseHandle(evt_req);
  CloseHandle(evt_rsp);
  UnmapViewOfFile(view);
  CloseHandle(map);
  real::cs2::detach_from_cs2(att.handle);
  real::cs2::stack::forensic_cleanup_ipc();
  return 0;
#endif
}
