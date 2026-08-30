// uefi_var.cpp — UEFI variable access implementation.
//
// Linux: /sys/firmware/efi/efivars (efivarfs).
// Windows: GetFirmwareEnvironmentVariable(Ex)A / SetFirmwareEnvironmentVariable(Ex)A
// with SeSystemEnvironmentPrivilege. Failures return structured errors.

#include "real/uefi/uefi_var.hpp"
#include "real/uefi/uefi_phys.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#  include <dirent.h>
#  include <sys/stat.h>
#elif LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#endif

namespace real::uefi {
namespace {

constexpr uint32_t kAttrNonVolatile       = 0x00000001;
constexpr uint32_t kAttrBootserviceAccess = 0x00000002;
constexpr uint32_t kAttrRuntimeAccess     = 0x00000004;
constexpr uint32_t kAttrTimeBasedAuth     = 0x00000020;

void global_guid(uint64_t& h0, uint64_t& h1) {
    efi_global_variable_guid(h0, h1);
}

std::string efivar_path(const std::string& name, uint64_t half0, uint64_t half1) {
    return "/sys/firmware/efi/efivars/" + name + "-" +
           efi_guid_halves_to_string(half0, half1);
}

#if LR_PLATFORM_LINUX
bool read_efivar_sysfs(const std::string& name, uint64_t half0, uint64_t half1,
                       std::vector<uint8_t>& data, uint32_t* attrs_out) {
    const std::string path = efivar_path(name, half0, half1);
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    std::vector<uint8_t> buf(64 * 1024);
    ssize_t bytes = read(fd, buf.data(), buf.size());
    close(fd);
    if (bytes < 4) return false;
    uint32_t attrs = 0;
    std::memcpy(&attrs, buf.data(), 4);
    if (attrs_out) *attrs_out = attrs;
    data.assign(buf.begin() + 4, buf.begin() + bytes);
    return true;
}
#endif

#if LR_PLATFORM_WINDOWS
Result<std::vector<uint8_t>> read_efivar_windows(const std::string& name,
                                                 uint64_t half0, uint64_t half1) {
    ensure_firmware_variable_privilege();
    const std::string guid = efi_guid_halves_to_windows_string(half0, half1);
    std::vector<uint8_t> buffer(64 * 1024);
    // Prefer Ex API when available (attributes)
    using GetExFn = DWORD (WINAPI*)(LPCSTR, LPCSTR, PVOID, DWORD, PDWORD);
    static GetExFn pGetEx = reinterpret_cast<GetExFn>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"),
                       "GetFirmwareEnvironmentVariableExA"));
    DWORD attrs = 0;
    DWORD size = 0;
    if (pGetEx) {
        size = pGetEx(name.c_str(), guid.c_str(), buffer.data(),
                      static_cast<DWORD>(buffer.size()), &attrs);
    } else {
        size = GetFirmwareEnvironmentVariableA(name.c_str(), guid.c_str(),
                                               buffer.data(),
                                               static_cast<DWORD>(buffer.size()));
    }
    if (!size) {
        DWORD err = GetLastError();
        if (err == ERROR_ENVVAR_NOT_FOUND)
            return Result<std::vector<uint8_t>>({}, "EFI variable not found: " + name);
        if (err == ERROR_INVALID_FUNCTION)
            return Result<std::vector<uint8_t>>({},
                "Firmware variables not supported (legacy BIOS or policy)");
        if (err == ERROR_PRIVILEGE_NOT_HELD)
            return Result<std::vector<uint8_t>>({},
                "SeSystemEnvironmentPrivilege required to read EFI variable " + name);
        auto oe = os_error("GetFirmwareEnvironmentVariable");
        return Result<std::vector<uint8_t>>({},
            std::string(oe.error_msg.c_str()) + " (" + name + ")");
    }
    buffer.resize(size);
    return Result<std::vector<uint8_t>>(std::move(buffer));
}

Result<void> write_efivar_windows(const std::string& name,
                                  uint64_t half0, uint64_t half1,
                                  uint32_t attributes,
                                  const std::vector<uint8_t>& data) {
    ensure_firmware_variable_privilege();
    const std::string guid = efi_guid_halves_to_windows_string(half0, half1);
    using SetExFn = BOOL (WINAPI*)(LPCSTR, LPCSTR, PVOID, DWORD, DWORD);
    static SetExFn pSetEx = reinterpret_cast<SetExFn>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"),
                       "SetFirmwareEnvironmentVariableExA"));
    BOOL ok = FALSE;
    if (pSetEx) {
        ok = pSetEx(name.c_str(), guid.c_str(),
                    data.empty() ? nullptr : const_cast<uint8_t*>(data.data()),
                    static_cast<DWORD>(data.size()), attributes);
    } else {
        ok = SetFirmwareEnvironmentVariableA(name.c_str(), guid.c_str(),
                    data.empty() ? nullptr : const_cast<uint8_t*>(data.data()),
                    static_cast<DWORD>(data.size()));
    }
    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_PRIVILEGE_NOT_HELD)
            return Result<void>("SeSystemEnvironmentPrivilege required to write EFI variable");
        if (err == ERROR_INVALID_FUNCTION)
            return Result<void>("Firmware variable write not supported on this platform");
        return os_error("SetFirmwareEnvironmentVariable");
    }
    return Result<void>();
}
#endif

bool read_u8_var(const char* name, uint64_t h0, uint64_t h1, uint8_t& out) {
    auto v = read_variable(name, h0, h1);
    if (!v || v->empty()) return false;
    out = (*v)[0];
    return true;
}

} // anonymous namespace

Result<std::vector<uint8_t>> read_variable(const std::string& name,
    uint64_t guid_hi, uint64_t guid_lo) {
#if LR_PLATFORM_LINUX
    std::vector<uint8_t> data;
    uint32_t attrs = 0;
    if (read_efivar_sysfs(name, guid_hi, guid_lo, data, &attrs)) {
        (void)attrs;
        return Result<std::vector<uint8_t>>(std::move(data));
    }
    return Result<std::vector<uint8_t>>({},
        "Cannot read UEFI variable " + name + " from efivarfs");
#elif LR_PLATFORM_WINDOWS
    return read_efivar_windows(name, guid_hi, guid_lo);
#else
    (void)name; (void)guid_hi; (void)guid_lo;
    return Result<std::vector<uint8_t>>({}, "UEFI variable read unsupported");
#endif
}

Result<void> write_variable(const std::string& name,
    uint64_t guid_hi, uint64_t guid_lo,
    uint32_t attributes, const std::vector<uint8_t>& data) {
#if LR_PLATFORM_LINUX
    const std::string path = efivar_path(name, guid_hi, guid_lo);
    // efivarfs requires immutable flag cleared for existing vars
    int fd = open(path.c_str(), O_WRONLY | O_CREAT, 0644);
    if (fd < 0)
        return Result<void>("Cannot open " + path + " for write (need root / efivarfs)");

    std::vector<uint8_t> output(sizeof(uint32_t) + data.size());
    std::memcpy(output.data(), &attributes, sizeof(uint32_t));
    if (!data.empty())
        std::memcpy(output.data() + sizeof(uint32_t), data.data(), data.size());

    ssize_t written = write(fd, output.data(), output.size());
    close(fd);
    if (written != static_cast<ssize_t>(output.size()))
        return Result<void>("Short write to " + path + " (immutable bit or auth failed?)");

    std::printf("[uefi] Variable %s written (%zu bytes, attrs=0x%x)\n",
                name.c_str(), data.size(), attributes);
    return Result<void>();
#elif LR_PLATFORM_WINDOWS
    auto r = write_efivar_windows(name, guid_hi, guid_lo, attributes, data);
    if (r) {
        std::printf("[uefi] Variable %s written (%zu bytes, attrs=0x%x)\n",
                    name.c_str(), data.size(), attributes);
    }
    return r;
#else
    (void)name; (void)guid_hi; (void)guid_lo; (void)attributes; (void)data;
    return Result<void>("UEFI variable write unsupported on this platform");
#endif
}

Result<void> delete_variable(const std::string& name,
    uint64_t guid_hi, uint64_t guid_lo) {
#if LR_PLATFORM_LINUX
    const std::string path = efivar_path(name, guid_hi, guid_lo);
    if (unlink(path.c_str()) != 0)
        return Result<void>("Cannot delete " + path + " (need root / clear immutable)");
    std::printf("[uefi] Variable %s deleted\n", name.c_str());
    return Result<void>();
#elif LR_PLATFORM_WINDOWS
    // Delete = Set with size 0
    auto r = write_efivar_windows(name, guid_hi, guid_lo,
        kAttrNonVolatile | kAttrBootserviceAccess | kAttrRuntimeAccess, {});
    if (r) std::printf("[uefi] Variable %s deleted (zero-length set)\n", name.c_str());
    return r;
#else
    (void)name; (void)guid_hi; (void)guid_lo;
    return Result<void>("UEFI variable delete unsupported");
#endif
}

Result<std::vector<EfiVariable>> enumerate_variables() {
    std::vector<EfiVariable> variables;
#if LR_PLATFORM_LINUX
    DIR* dir = opendir("/sys/firmware/efi/efivars");
    if (!dir)
        return Result<std::vector<EfiVariable>>({},
            "Cannot open /sys/firmware/efi/efivars (not UEFI or no efivarfs)");

    while (dirent* entry = readdir(dir)) {
        if (entry->d_name[0] == '.') continue;
        std::string fname(entry->d_name);
        EfiVariable var;
        if (!parse_efivar_filename(fname, var.name, var.vendor_guid_hi, var.vendor_guid_lo))
            continue;
        uint32_t attrs = 0;
        if (!read_efivar_sysfs(var.name, var.vendor_guid_hi, var.vendor_guid_lo,
                               var.data, &attrs))
            continue;
        var.attributes = attrs;
        variables.push_back(std::move(var));
    }
    closedir(dir);
#elif LR_PLATFORM_WINDOWS
    // Windows does not expose GetNextVariableName to usermode. Probe known
    // security-relevant Global Variable GUID names and report what is readable.
    uint64_t h0 = 0, h1 = 0;
    global_guid(h0, h1);
    static const char* kKnown[] = {
        "SecureBoot", "SetupMode", "AuditMode", "DeployedMode",
        "PK", "KEK", "db", "dbx", "dbt", "dbr",
        "VendorKeys", "OsIndications", "OsIndicationsSupported",
        "BootOrder", "BootCurrent", "Timeout",
        "PlatformLang", "Lang", "SignatureSupport",
    };
    for (const char* n : kKnown) {
        auto data = read_efivar_windows(n, h0, h1);
        if (!data) continue;
        EfiVariable var;
        var.name = n;
        var.vendor_guid_hi = h0;
        var.vendor_guid_lo = h1;
        var.attributes = kAttrNonVolatile | kAttrBootserviceAccess | kAttrRuntimeAccess;
        var.data = std::move(*data);
        variables.push_back(std::move(var));
    }
    if (variables.empty()) {
        return Result<std::vector<EfiVariable>>({},
            "No EFI variables readable (need UEFI + SeSystemEnvironmentPrivilege)");
    }
#else
    return Result<std::vector<EfiVariable>>({}, "UEFI variable enumeration unsupported");
#endif
    std::printf("[uefi] Enumerated %zu UEFI variables\n", variables.size());
    return Result<std::vector<EfiVariable>>(std::move(variables));
}

Result<bool> secure_boot_variables_writable() {
    auto state = get_secure_boot_state();
    if (!state)
        return Result<bool>(false, state.error_msg);
    // PK is rewritable without auth only in Setup Mode (no platform key enrolled).
    if (state->setup_mode)
        return Result<bool>(true);
    // Deployed mode / Secure Boot on => authenticated writes only.
    return Result<bool>(false);
}

Result<SecureBootState> get_secure_boot_state() {
    SecureBootState state;
    uint64_t h0 = 0, h1 = 0;
    global_guid(h0, h1);

    uint8_t v = 0;
    if (read_u8_var("SecureBoot", h0, h1, v))
        state.secure_boot_enabled = (v != 0);
    if (read_u8_var("SetupMode", h0, h1, v))
        state.setup_mode = (v != 0);
    if (read_u8_var("AuditMode", h0, h1, v))
        state.audit_mode = (v != 0);
    const bool got_deployed = read_u8_var("DeployedMode", h0, h1, v);
    if (got_deployed) state.deployed_mode = (v != 0);

    auto pk = read_variable("PK", h0, h1);
    if (pk) state.pk = std::move(*pk);
    auto kek = read_variable("KEK", h0, h1);
    if (kek) state.kek = std::move(*kek);
    auto db = read_variable("db", h0, h1);
    if (db) state.db = std::move(*db);
    auto dbx = read_variable("dbx", h0, h1);
    if (dbx) state.dbx = std::move(*dbx);

    // DeployedMode optional: only infer when we actually observed SetupMode or PK.
    if (!got_deployed && (pk || state.setup_mode || state.secure_boot_enabled))
        state.deployed_mode = !state.setup_mode && !state.pk.empty();

    const bool got_sb = read_variable("SecureBoot", h0, h1).ok;
    const bool got_setup = read_variable("SetupMode", h0, h1).ok;
    const bool any =
        got_sb || got_setup || got_deployed || state.audit_mode ||
        !state.pk.empty() || !state.kek.empty() || !state.db.empty() || !state.dbx.empty();

    std::printf("[uefi] Secure Boot: %s, Setup Mode: %s, Audit: %s, Deployed: %s "
                "(PK=%zu KEK=%zu db=%zu dbx=%zu)\n",
                state.secure_boot_enabled ? "ON" : "OFF",
                state.setup_mode ? "ON" : "OFF",
                state.audit_mode ? "ON" : "OFF",
                state.deployed_mode ? "ON" : "OFF",
                state.pk.size(), state.kek.size(), state.db.size(), state.dbx.size());

    if (!any) {
        // Still return the zeroed structure with ok=false so callers can show errors.
        return Result<SecureBootState>(state,
            "Secure Boot variables not accessible (privilege or non-UEFI platform)");
    }
    return Result<SecureBootState>(std::move(state));
}

Result<void> spoof_secure_boot_variable(const std::string& name,
    const std::vector<uint8_t>& forged_data) {
    std::printf("[uefi] Spoofing Secure Boot variable: %s (%zu bytes)\n",
                name.c_str(), forged_data.size());
    std::printf("[uefi] SCAR: Variable write is logged by firmware / measured boot\n");
    std::printf("[uefi] MITIGATION: TPM PCR extends detect variable store changes\n");

    uint64_t h0 = 0, h1 = 0;
    global_guid(h0, h1);

    // Authenticated variables typically need TIME_BASED_AUTHENTICATED_WRITE_ACCESS
    uint32_t attrs = kAttrNonVolatile | kAttrBootserviceAccess | kAttrRuntimeAccess;
    if (name == "PK" || name == "KEK" || name == "db" || name == "dbx" ||
        name == "dbt" || name == "dbr") {
        attrs |= kAttrTimeBasedAuth;
    }

    auto r = write_variable(name, h0, h1, attrs, forged_data);
    if (!r) {
        // Honest failure — do not claim success
        return Result<void>(std::string("spoof_secure_boot_variable failed: ") +
                            r.error_msg.c_str());
    }
    return Result<void>();
}

// ── Pure helper used by unit tests: map raw SecureBoot/SetupMode bytes ──

SecureBootState map_secure_boot_fields(uint8_t secure_boot, uint8_t setup_mode,
                                       uint8_t audit_mode, uint8_t deployed_mode,
                                       std::vector<uint8_t> pk,
                                       std::vector<uint8_t> kek,
                                       std::vector<uint8_t> db,
                                       std::vector<uint8_t> dbx) {
    SecureBootState s;
    s.secure_boot_enabled = secure_boot != 0;
    s.setup_mode = setup_mode != 0;
    s.audit_mode = audit_mode != 0;
    s.deployed_mode = deployed_mode != 0;
    s.pk = std::move(pk);
    s.kek = std::move(kek);
    s.db = std::move(db);
    s.dbx = std::move(dbx);
    return s;
}

} // namespace real::uefi
