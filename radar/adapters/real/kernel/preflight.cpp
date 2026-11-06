// preflight.cpp — best-effort Windows probes for the pre-attach report (E61).

#include "real/kernel/preflight.hpp"
#include "real/platform.hpp"

#include <filesystem>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include <winreg.h>
#endif

namespace real::kernel::preflight {
namespace {

#if LR_PLATFORM_WINDOWS
bool read_dword(const wchar_t* subkey, const wchar_t* value, bool& out) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key) !=
      ERROR_SUCCESS) {
    return false;
  }
  DWORD data = 0;
  DWORD size = sizeof(data);
  DWORD type = 0;
  const LONG status =
      RegQueryValueExW(key, value, nullptr, &type,
                       reinterpret_cast<LPBYTE>(&data), &size);
  RegCloseKey(key);
  if (status != ERROR_SUCCESS || type != REG_DWORD) return false;
  out = data != 0;
  return true;
}

bool is_admin() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
  TOKEN_ELEVATION elevation{};
  DWORD size = sizeof(elevation);
  const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation,
                                      sizeof(elevation), &size);
  CloseHandle(token);
  return ok && elevation.TokenIsElevated != 0;
}
#endif

}  // namespace

SystemStatus probe(const char* driver_path) {
  SystemStatus status;
#if LR_PLATFORM_WINDOWS
  status.is_admin = is_admin();
  status.is_admin_known = true;

  bool value = false;
  if (read_dword(L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State",
                 L"UEFISecureBootEnabled", value)) {
    status.secure_boot = value;
    status.secure_boot_known = true;
  }
  if (read_dword(
          L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\"
          L"HypervisorEnforcedCodeIntegrity",
          L"Enabled", value)) {
    status.hvci = value;
    status.hvci_known = true;
  }
  if (read_dword(L"SYSTEM\\CurrentControlSet\\Control\\CI\\Config",
                 L"VulnerableDriverBlocklistEnable", value)) {
    status.blocklist = value;
    status.blocklist_known = true;
  }
  // Test signing is not exposed by a stable registry key; report unknown.
  status.test_signing_known = false;
#endif
  if (driver_path != nullptr && *driver_path != '\0') {
    std::error_code error;
    status.driver_file = std::filesystem::is_regular_file(driver_path, error);
    status.driver_file_known = true;
  }
  return status;
}

}  // namespace real::kernel::preflight
