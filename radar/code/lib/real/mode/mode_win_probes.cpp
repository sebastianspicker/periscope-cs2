// mode_win_probes.cpp — Windows device/registry/SetupAPI tier probes.

#include "real/mode/mode_internal.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#include "real/win/xorstr.hpp"
#include <setupapi.h>
#if LR_COMPILER_MSVC
#pragma comment(lib, "setupapi.lib")
#endif
#include <cstdio>
#include <cstring>

namespace real::mode::detail {

using real::mode::detail::contains_ci;

bool win_try_open_device(const char* path) {
  if (!path || !*path) return false;
  auto& api = real::win::g_Api();
  api.ensure_resolved();
  if (!api.CreateFileA || !api.CloseHandle) return false;
  HANDLE h = api.CreateFileA(path, GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == nullptr || h == INVALID_HANDLE_VALUE) return false;
  api.CloseHandle(h);
  return true;
}

bool win_reg_key_exists(const wchar_t* subkey) {
  auto& api = real::win::g_Api();
  api.ensure_resolved();
  if (!api.RegOpenKeyExW || !api.RegCloseKey) return false;
  HKEY key = nullptr;
  const LONG st = api.RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key);
  if (st != ERROR_SUCCESS || !key) return false;
  api.RegCloseKey(key);
  return true;
}

bool win_service_registered(const char* service_name) {
  if (!service_name || !*service_name) return false;
  auto& api = real::win::g_Api();
  api.ensure_resolved();
  if (!api.OpenSCManagerA || !api.OpenServiceA || !api.CloseHandle) return false;
  // SC_MANAGER_CONNECT = 0x0001
  HANDLE scm = api.OpenSCManagerA(nullptr, nullptr, 0x0001);
  if (!scm) return false;
  // SERVICE_QUERY_STATUS = 0x0004
  HANDLE svc = api.OpenServiceA(scm, service_name, 0x0004);
  const bool ok = (svc != nullptr && svc != INVALID_HANDLE_VALUE);
  if (ok) api.CloseHandle(svc);
  api.CloseHandle(scm);
  return ok;
}

bool win_token_elevated() {
  auto& api = real::win::g_Api();
  api.ensure_resolved();
  if (!api.OpenProcessToken || !api.GetTokenInformation || !api.CloseHandle) {
    return false;
  }
  HANDLE hToken = nullptr;
  // NtCurrentProcess pseudo-handle = (HANDLE)-1
  if (!api.OpenProcessToken(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)),
                            TOKEN_QUERY, &hToken) ||
      !hToken) {
    return false;
  }
  TOKEN_ELEVATION elev{};
  DWORD size = sizeof(elev);
  bool elevated = false;
  if (api.GetTokenInformation(hToken, TokenElevation, &elev, size, &size)) {
    elevated = elev.TokenIsElevated != 0;
  }
  api.CloseHandle(hToken);
  return elevated;
}

// Known BYOVD / physmem device object names used in the research curriculum.
// Honest availability requires a live open, not merely admin rights.
const char* const kByovdDevicePaths[] = {
    "\\\\.\\gdrv",
    "\\\\.\\RTCore64",
    "\\\\.\\RTCore32",
    "\\\\.\\WinRing0_1_2_0",
    "\\\\.\\WinRing0",
    "\\\\.\\EneIo64",
    "\\\\.\\EneIo",
    "\\\\.\\WinIo64",
    "\\\\.\\WinIo",
    "\\\\.\\AsIO3",
    "\\\\.\\AsIO",
    "\\\\.\\GLCKIo2",
    "\\\\.\\dbutil_2_3",
    "\\\\.\\DBUtil_2_3",
    "\\\\.\\LegitRadarPhys",
    "\\\\.\\PhysicalMemory",
    "\\\\.\\HqHw64",
    "\\\\.\\MsIo64",
};

const char* const kByovdServiceNames[] = {
    "gdrv",
    "RTCore64",
    "WinRing0_1_2_0",
    "EneIo64",
    "WinIo",
    "AsIO3",
    "GLCKIo2",
    "dbutil_2_3",
    "mhyprot2",
    "mhyprot3",
};

// DMA / FPGA style device object names (T4 path without linking lib/real/dma).
const char* const kDmaDevicePaths[] = {
    "\\\\.\\PCILeech",
    "\\\\.\\FPGA0",
    "\\\\.\\FTD3XX",
    "\\\\.\\rawudp",
    "\\\\.\\FTDIX",
};

// Intel Thunderbolt / USB4 controller PCI device IDs commonly seen on laptops.
bool win_hwid_is_thunderbolt_or_usb4(const char* hwid, const char* desc) {
  if (desc) {
    if (contains_ci(desc, "Thunderbolt") || contains_ci(desc, "USB4") ||
        contains_ci(desc, "USB 4")) {
      return true;
    }
  }
  if (!hwid) return false;
  static const char* const kIds[] = {
      "VEN_8086&DEV_15D2",  // Alpine Ridge
      "VEN_8086&DEV_15D3",
      "VEN_8086&DEV_15EA",  // Titan Ridge
      "VEN_8086&DEV_15EB",
      "VEN_8086&DEV_1134",  // Maple Ridge
      "VEN_8086&DEV_1137",
      "VEN_8086&DEV_A0EC",  // Tiger Lake USB4
      "VEN_8086&DEV_A0ED",
      "VEN_8086&DEV_9A1B",  // Ice Lake
      "VEN_8086&DEV_9A1D",
      "VEN_8086&DEV_466D",  // Alder Lake
      "VEN_8086&DEV_7E7D",  // Meteor Lake
  };
  for (const char* id : kIds) {
    if (contains_ci(hwid, id)) return true;
  }
  return false;
}

// Xilinx / Altera / Lattice / Broadcom NetXtreme-style research DMA vendors.
bool win_hwid_is_fpga_dma_vendor(const char* hwid, const char* desc) {
  if (desc) {
    if (contains_ci(desc, "Xilinx") || contains_ci(desc, "Altera") ||
        contains_ci(desc, "Lattice") || contains_ci(desc, "PCILeech") ||
        contains_ci(desc, "Screamer") || contains_ci(desc, "EnigmaX1") ||
        contains_ci(desc, "FTDI")) {
      return true;
    }
  }
  if (!hwid) return false;
  static const char* const kVendors[] = {
      "VEN_10EE",  // Xilinx
      "VEN_1172",  // Altera
      "VEN_1204",  // Lattice
      "VEN_1D50",  // OpenMoko / research
      "VEN_0403",  // FTDI (common FPGA bridge)
      "VEN_1B36",  // Red Hat / QEMU vfio research
  };
  for (const char* v : kVendors) {
    if (contains_ci(hwid, v)) return true;
  }
  return false;
}

bool win_setupapi_dma_signals(std::string& reason) {
  HDEVINFO info = SetupDiGetClassDevsA(nullptr, "PCI", nullptr,
                                       DIGCF_PRESENT | DIGCF_ALLCLASSES);
  if (info == INVALID_HANDLE_VALUE) {
    reason = "SetupDiGetClassDevs(PCI) failed";
    return false;
  }

  SP_DEVINFO_DATA data{};
  data.cbSize = sizeof(data);
  bool found = false;
  char matched[256] = {};

  for (DWORD i = 0; SetupDiEnumDeviceInfo(info, i, &data); ++i) {
    char desc[256] = {};
    char hwid[512] = {};
    SetupDiGetDeviceRegistryPropertyA(
        info, &data, SPDRP_DEVICEDESC, nullptr,
        reinterpret_cast<PBYTE>(desc), sizeof(desc), nullptr);
    SetupDiGetDeviceRegistryPropertyA(
        info, &data, SPDRP_HARDWAREID, nullptr,
        reinterpret_cast<PBYTE>(hwid), sizeof(hwid), nullptr);

    // PCI base-class 0x08 subclass 0x80 = "Other system peripheral" (DMA ctrl)
    // Hardware IDs do not always encode class; also match CC_0580 when present.
    if (contains_ci(hwid, "CC_0580") || contains_ci(hwid, "CC_0880")) {
      found = true;
      std::snprintf(matched, sizeof(matched), "PCI class DMA peripheral (%s)",
                    hwid[0] ? hwid : desc);
      break;
    }
    if (win_hwid_is_thunderbolt_or_usb4(hwid, desc)) {
      found = true;
      std::snprintf(matched, sizeof(matched), "Thunderbolt/USB4 PCI device (%s)",
                    desc[0] ? desc : hwid);
      break;
    }
    if (win_hwid_is_fpga_dma_vendor(hwid, desc)) {
      found = true;
      std::snprintf(matched, sizeof(matched), "FPGA/DMA-class PCI device (%s)",
                    desc[0] ? desc : hwid);
      break;
    }
  }
  SetupDiDestroyDeviceInfoList(info);

  if (found) {
    reason = matched;
    return true;
  }
  reason = "no Thunderbolt/USB4/FPGA/DMA-class PCI device present";
  return false;
}


size_t byovd_device_path_count() {
  return sizeof(kByovdDevicePaths) / sizeof(kByovdDevicePaths[0]);
}
size_t byovd_service_name_count() {
  return sizeof(kByovdServiceNames) / sizeof(kByovdServiceNames[0]);
}
size_t dma_device_path_count() {
  return sizeof(kDmaDevicePaths) / sizeof(kDmaDevicePaths[0]);
}

}  // namespace real::mode::detail
#endif  // LR_PLATFORM_WINDOWS
