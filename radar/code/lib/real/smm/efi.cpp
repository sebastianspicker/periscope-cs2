// efi.cpp — EFI runtime-services and variable backend.
//
// TECHNIQUE: Locate EFI system/runtime tables; Get/SetVariable (often SMI).
//
// SCAR: SetVariable audited by Secure Boot / HVCI; PCR updates on auth vars.
//
// BLUE: Variable write telemetry; authenticate Boot#### / db / KEK changes.
//
// MITIGATION: Authenticated variables, Secure Boot, measured boot.

#include "real/smm/smm_interface.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <dirent.h>
#  include <fcntl.h>
#  include <unistd.h>
#elif LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#endif

namespace real::smm {
namespace {

std::vector<std::uint8_t> read_file(const std::string& path) {
  FILE* file = std::fopen(path.c_str(), "rb");
  if (!file) return {};
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::rewind(file);
  if (size <= 0) {
    std::fclose(file);
    return {};
  }
  std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
  std::fread(data.data(), 1, data.size(), file);
  std::fclose(file);
  return data;
}

void format_guid_path(char* path, std::size_t path_len, const char* name,
                      std::uint64_t vendor_guid_hi, std::uint64_t vendor_guid_lo) {
  std::snprintf(
      path, path_len,
      "/sys/firmware/efi/efivars/%s-%08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x",
      name, static_cast<unsigned>(vendor_guid_hi >> 32),
      static_cast<unsigned>((vendor_guid_hi >> 16) & 0xFFFF),
      static_cast<unsigned>(vendor_guid_hi & 0xFFFF),
      static_cast<unsigned>((vendor_guid_lo >> 56) & 0xFF),
      static_cast<unsigned>((vendor_guid_lo >> 48) & 0xFF),
      static_cast<unsigned>((vendor_guid_lo >> 40) & 0xFF),
      static_cast<unsigned>((vendor_guid_lo >> 32) & 0xFF),
      static_cast<unsigned>((vendor_guid_lo >> 24) & 0xFF),
      static_cast<unsigned>((vendor_guid_lo >> 16) & 0xFF),
      static_cast<unsigned>((vendor_guid_lo >> 8) & 0xFF),
      static_cast<unsigned>(vendor_guid_lo & 0xFF));
}

#if LR_PLATFORM_WINDOWS
GUID guid_from_parts(std::uint64_t hi, std::uint64_t lo) {
  GUID guid{};
  guid.Data1 = static_cast<std::uint32_t>(hi >> 32);
  guid.Data2 = static_cast<std::uint16_t>((hi >> 16) & 0xFFFF);
  guid.Data3 = static_cast<std::uint16_t>(hi & 0xFFFF);
  std::memcpy(guid.Data4, &lo, sizeof(guid.Data4));
  return guid;
}

// Format GUID as registry-style string for GetFirmwareEnvironmentVariableA.
// Windows expects the GUID string as the "namespace" parameter.
void guid_to_string(const GUID& g, char* out, std::size_t out_len) {
  std::snprintf(out, out_len,
                "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                static_cast<unsigned long>(g.Data1), g.Data2, g.Data3,
                g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4],
                g.Data4[5], g.Data4[6], g.Data4[7]);
}
#endif

}  // namespace

Result<std::uint64_t> get_efi_runtime_services() {
#if LR_PLATFORM_LINUX
  auto data = read_file("/sys/firmware/efi/systab");
  if (data.empty()) {
    return Result<std::uint64_t>(0, "No EFI runtime (not a UEFI system)");
  }
  std::string text(reinterpret_cast<char*>(data.data()), data.size());
  const auto pos = text.find("RUNTIME=");
  if (pos == std::string::npos) {
    return Result<std::uint64_t>(0, "RUNTIME= not in systab");
  }
  std::uint64_t address = 0;
  std::sscanf(text.c_str() + static_cast<std::ptrdiff_t>(pos), "RUNTIME=0x%llx",
              reinterpret_cast<unsigned long long*>(&address));
  if (!address) return Result<std::uint64_t>(0, "RUNTIME address is zero");
  return address;
#elif LR_PLATFORM_WINDOWS
  // Usermode cannot dereference EFI runtime table; presence of firmware
  // environment API indicates runtime services are wired through the kernel.
  HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
  if (!kernel32) {
    return Result<std::uint64_t>(
        0, "EFI runtime services not accessible from usermode on Windows");
  }
  auto pGet = reinterpret_cast<void*>(
      GetProcAddress(kernel32, "GetFirmwareEnvironmentVariableA"));
  if (!pGet) {
    return Result<std::uint64_t>(0, "GetFirmwareEnvironmentVariableA missing");
  }
  // Return non-zero sentinel meaning "available via kernel thunk", not a PA.
  return Result<std::uint64_t>(
      0, "EFI runtime: physical table requires kernel physmem; API available");
#else
  return Result<std::uint64_t>(0, "EFI runtime requires UEFI firmware");
#endif
}

Result<std::uint64_t> get_efi_system_table() {
#if LR_PLATFORM_LINUX
  auto data = read_file("/sys/firmware/efi/systab");
  if (data.empty()) return Result<std::uint64_t>(0, "No EFI systab");
  std::string text(reinterpret_cast<char*>(data.data()), data.size());
  const auto pos = text.find("EFI_SYSTEM_TABLE=");
  if (pos == std::string::npos) {
    return Result<std::uint64_t>(0, "EFI_SYSTEM_TABLE= not found");
  }
  std::uint64_t address = 0;
  std::sscanf(text.c_str() + static_cast<std::ptrdiff_t>(pos),
              "EFI_SYSTEM_TABLE=0x%llx",
              reinterpret_cast<unsigned long long*>(&address));
  if (!address) return Result<std::uint64_t>(0, "EFI_SYSTEM_TABLE address is zero");
  return address;
#elif LR_PLATFORM_WINDOWS
  return Result<std::uint64_t>(
      0, "EFI system table requires kernel-mode physical memory access");
#else
  return Result<std::uint64_t>(0, "EFI system table not accessible");
#endif
}

Result<void> efi_set_variable(const char* name, const std::vector<std::uint8_t>& data,
                              std::uint64_t vendor_guid_hi,
                              std::uint64_t vendor_guid_lo,
                              std::uint32_t attributes) {
  if (!name || !*name) return Result<void>("EFI variable name required");
  std::printf("[smm] efi_set_variable: name=%s size=%zu\n", name, data.size());

#if LR_PLATFORM_WINDOWS
  GUID guid = guid_from_parts(vendor_guid_hi, vendor_guid_lo);
  char guid_str[64];
  guid_to_string(guid, guid_str, sizeof(guid_str));
  // Prefer Ex API when available (attributes).
  using SetVarEx = BOOL(WINAPI*)(LPCSTR, LPCSTR, DWORD, PVOID, DWORD);
  HMODULE k32 = GetModuleHandleA("kernel32.dll");
  auto set_ex = k32 ? reinterpret_cast<SetVarEx>(
                          GetProcAddress(k32, "SetFirmwareEnvironmentVariableExA"))
                    : nullptr;
  if (set_ex) {
    if (!set_ex(name, guid_str, attributes, const_cast<std::uint8_t*>(data.data()),
                static_cast<DWORD>(data.size()))) {
      return os_error("SetFirmwareEnvironmentVariableExA");
    }
  } else {
    if (!SetFirmwareEnvironmentVariableA(
            name, guid_str, const_cast<std::uint8_t*>(data.data()),
            static_cast<DWORD>(data.size()))) {
      return os_error("SetFirmwareEnvironmentVariableA");
    }
  }
  std::printf("[smm] EFI variable set successfully\n");
  return Result<void>();

#elif LR_PLATFORM_LINUX
  char path[512];
  format_guid_path(path, sizeof(path), name, vendor_guid_hi, vendor_guid_lo);
  int fd = open(path, O_WRONLY | O_CREAT, 0644);
  if (fd < 0) return os_error("open efivar for write (need root)");
  std::uint32_t attr_le = attributes;
  const bool ok =
      write(fd, &attr_le, sizeof(attr_le)) == static_cast<ssize_t>(sizeof(attr_le)) &&
      (data.empty() ||
       write(fd, data.data(), data.size()) == static_cast<ssize_t>(data.size()));
  close(fd);
  if (!ok) return os_error("write efivar");
  std::printf("[smm] EFI variable written to %s\n", path);
  return Result<void>();
#else
  (void)name;
  (void)data;
  (void)vendor_guid_hi;
  (void)vendor_guid_lo;
  (void)attributes;
  return Result<void>("EFI variable set requires Linux or Windows");
#endif
}

Result<std::vector<std::uint8_t>> efi_get_variable(const char* name,
                                                    std::uint64_t vendor_guid_hi,
                                                    std::uint64_t vendor_guid_lo) {
  if (!name || !*name) {
    return Result<std::vector<std::uint8_t>>({}, "EFI variable name required");
  }

#if LR_PLATFORM_WINDOWS
  GUID guid = guid_from_parts(vendor_guid_hi, vendor_guid_lo);
  char guid_str[64];
  guid_to_string(guid, guid_str, sizeof(guid_str));
  std::vector<std::uint8_t> buffer(4096);
  const DWORD size = GetFirmwareEnvironmentVariableA(
      name, guid_str, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (!size) {
    const DWORD err = GetLastError();
    if (err == ERROR_ENVVAR_NOT_FOUND) {
      return Result<std::vector<std::uint8_t>>({}, "EFI variable not found");
    }
    auto error = os_error("GetFirmwareEnvironmentVariableA");
    return Result<std::vector<std::uint8_t>>({}, error.error_msg);
  }
  buffer.resize(size);
  return buffer;

#elif LR_PLATFORM_LINUX
  char path[512];
  format_guid_path(path, sizeof(path), name, vendor_guid_hi, vendor_guid_lo);
  auto data = read_file(path);
  if (data.empty()) {
    return Result<std::vector<std::uint8_t>>({}, "EFI variable not found");
  }
  if (data.size() <= 4) return std::vector<std::uint8_t>{};
  return std::vector<std::uint8_t>(data.begin() + 4, data.end());
#else
  (void)name;
  (void)vendor_guid_hi;
  (void)vendor_guid_lo;
  return Result<std::vector<std::uint8_t>>(
      {}, "EFI variable read requires Linux or Windows");
#endif
}

Result<std::vector<std::string>> efi_list_variables() {
  std::vector<std::string> names;
#if LR_PLATFORM_LINUX
  DIR* dir = opendir("/sys/firmware/efi/efivars");
  if (!dir) {
    auto err = os_error("opendir efivars");
    return Result<std::vector<std::string>>({}, err.error_msg);
  }
  while (dirent* ent = readdir(dir)) {
    if (ent->d_name[0] == '.') continue;
    names.emplace_back(ent->d_name);
  }
  closedir(dir);
  return names;
#elif LR_PLATFORM_WINDOWS
  // Windows has no simple usermode enumerate; probe a few research-relevant names.
  static const char* kProbe[] = {
      "BootOrder", "SecureBoot", "SetupMode", "PK", "KEK", "db", "dbx",
      "Timeout", "PlatformLang",
  };
  // EFI Global Variable GUID
  constexpr std::uint64_t kGlobalHi = 0x8BE4DF61ULL << 32 | 0x93CA11d2ULL;
  constexpr std::uint64_t kGlobalLo = 0xAA0D00E098032B8CULL;
  for (const char* n : kProbe) {
    auto v = efi_get_variable(n, kGlobalHi, kGlobalLo);
    if (v) names.emplace_back(n);
  }
  return names;
#else
  return Result<std::vector<std::string>>(
      {}, "EFI variable list requires Linux or Windows");
#endif
}

}  // namespace real::smm
