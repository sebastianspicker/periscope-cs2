// uefi_phys.cpp — Shared physical / firmware-table read backend.

#include "real/uefi/uefi_phys.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
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

struct Blob {
    std::string signature;
    std::vector<uint8_t> data;
};

std::mutex g_mu;
std::map<uint64_t, Blob> g_pa_cache;           // pa -> blob
std::map<std::string, uint64_t> g_sig_to_pa;   // signature -> pa
std::map<uint64_t, std::vector<uint8_t>> g_injects;
PhysReadHook g_hook;
uint64_t g_next_synth_pa = 0xACE000000000ull;

std::vector<uint8_t> read_file_bytes(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return {};
    if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return {}; }
    long sz = std::ftell(f);
    if (sz <= 0) { std::fclose(f); return {}; }
    std::rewind(f);
    std::vector<uint8_t> out(static_cast<size_t>(sz));
    size_t n = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    out.resize(n);
    return out;
}

bool read_from_maps_locked(uint64_t pa, void* buf, size_t size) {
    // Exact inject region that fully covers the request
    for (const auto& kv : g_injects) {
        const uint64_t base = kv.first;
        const auto& data = kv.second;
        if (pa >= base && pa + size <= base + data.size()) {
            std::memcpy(buf, data.data() + (pa - base), size);
            return true;
        }
    }
    // Cache: allow sub-range reads
    for (const auto& kv : g_pa_cache) {
        const uint64_t base = kv.first;
        const auto& data = kv.second.data;
        if (pa >= base && pa + size <= base + data.size()) {
            std::memcpy(buf, data.data() + (pa - base), size);
            return true;
        }
    }
    return false;
}

#if LR_PLATFORM_LINUX
bool read_dev_mem(uint64_t pa, void* buf, size_t size) {
    static int fd = -2; // -2 uninit, -1 failed, >=0 open
    if (fd == -2) {
        fd = open("/dev/mem", O_RDONLY | O_SYNC);
    }
    if (fd < 0) return false;
    return pread(fd, buf, size, static_cast<off_t>(pa)) == static_cast<ssize_t>(size);
}
#endif

uint32_t sig_to_fourcc(const char* signature) {
    uint32_t v = 0;
    for (int i = 0; i < 4 && signature[i]; ++i)
        v |= static_cast<uint32_t>(static_cast<uint8_t>(signature[i])) << (8 * i);
    return v;
}

std::string fourcc_to_sig(uint32_t fourcc) {
    char s[5] = {
        static_cast<char>(fourcc & 0xFF),
        static_cast<char>((fourcc >> 8) & 0xFF),
        static_cast<char>((fourcc >> 16) & 0xFF),
        static_cast<char>((fourcc >> 24) & 0xFF),
        0
    };
    return std::string(s, 4);
}

} // namespace

bool acpi_checksum_valid(const uint8_t* data, size_t size) noexcept {
    if (!data || size == 0) return false;
    uint8_t sum = 0;
    for (size_t i = 0; i < size; ++i) sum = static_cast<uint8_t>(sum + data[i]);
    return sum == 0;
}

void acpi_fix_checksum(uint8_t* data, size_t size) noexcept {
    if (!data || size < 10) return;
    data[9] = 0;
    uint8_t sum = 0;
    for (size_t i = 0; i < size; ++i) sum = static_cast<uint8_t>(sum + data[i]);
    data[9] = static_cast<uint8_t>((256 - sum) & 0xFF);
}

void efi_guid_to_halves(uint32_t data1, uint16_t data2, uint16_t data3,
                        const uint8_t data4[8],
                        uint64_t& half0, uint64_t& half1) noexcept {
    uint8_t raw[16]{};
    std::memcpy(raw + 0, &data1, 4);
    std::memcpy(raw + 4, &data2, 2);
    std::memcpy(raw + 6, &data3, 2);
    std::memcpy(raw + 8, data4, 8);
    std::memcpy(&half0, raw + 0, 8);
    std::memcpy(&half1, raw + 8, 8);
}

void efi_guid_from_halves(uint64_t half0, uint64_t half1,
                          uint32_t& data1, uint16_t& data2, uint16_t& data3,
                          uint8_t data4[8]) noexcept {
    uint8_t raw[16]{};
    std::memcpy(raw + 0, &half0, 8);
    std::memcpy(raw + 8, &half1, 8);
    std::memcpy(&data1, raw + 0, 4);
    std::memcpy(&data2, raw + 4, 2);
    std::memcpy(&data3, raw + 6, 2);
    std::memcpy(data4, raw + 8, 8);
}

std::string format_efi_guid(uint32_t data1, uint16_t data2, uint16_t data3,
                            const uint8_t data4[8]) {
    char buf[64];
    std::snprintf(buf, sizeof(buf),
        "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        data1, data2, data3,
        data4[0], data4[1],
        data4[2], data4[3], data4[4], data4[5], data4[6], data4[7]);
    return buf;
}

std::string efi_guid_halves_to_string(uint64_t half0, uint64_t half1) {
    uint32_t d1; uint16_t d2, d3; uint8_t d4[8];
    efi_guid_from_halves(half0, half1, d1, d2, d3, d4);
    return format_efi_guid(d1, d2, d3, d4);
}

std::string efi_guid_halves_to_windows_string(uint64_t half0, uint64_t half1) {
    return "{" + efi_guid_halves_to_string(half0, half1) + "}";
}

void efi_global_variable_guid(uint64_t& half0, uint64_t& half1) noexcept {
    static const uint8_t d4[8] = {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C};
    efi_guid_to_halves(0x8BE4DF61u, 0x93CAu, 0x11D2u, d4, half0, half1);
}

bool parse_efivar_filename(const std::string& filename,
                           std::string& out_name,
                           uint64_t& out_half0, uint64_t& out_half1) {
    // Format: Name-XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    auto dash = filename.find('-');
    if (dash == std::string::npos || dash == 0) return false;
    // GUID is last 36 chars after final name segment: find GUID pattern length 36
    if (filename.size() < 38) return false; // 1 + '-' + 36
    // rfind the start of GUID: 36 chars at end
    size_t guid_start = filename.size() - 36;
    if (guid_start == 0 || filename[guid_start - 1] != '-') return false;
    out_name = filename.substr(0, guid_start - 1);
    const std::string g = filename.substr(guid_start);
    // XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    unsigned d1 = 0, d2 = 0, d3 = 0, a = 0, b = 0;
    unsigned c0=0,c1=0,c2=0,c3=0,c4=0,c5=0;
    if (std::sscanf(g.c_str(),
            "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            &d1, &d2, &d3, &a, &b, &c0, &c1, &c2, &c3, &c4, &c5) != 11) {
        return false;
    }
    uint8_t d4[8] = {
        static_cast<uint8_t>(a), static_cast<uint8_t>(b),
        static_cast<uint8_t>(c0), static_cast<uint8_t>(c1),
        static_cast<uint8_t>(c2), static_cast<uint8_t>(c3),
        static_cast<uint8_t>(c4), static_cast<uint8_t>(c5)
    };
    efi_guid_to_halves(d1, static_cast<uint16_t>(d2), static_cast<uint16_t>(d3),
                       d4, out_half0, out_half1);
    return true;
}

void set_phys_read_hook(PhysReadHook hook) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_hook = std::move(hook);
}

void inject_phys_region(uint64_t pa, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_injects[pa] = data;
}

void clear_phys_injects() {
    std::lock_guard<std::mutex> lock(g_mu);
    g_injects.clear();
    g_hook = nullptr;
}

void clear_firmware_cache() {
    std::lock_guard<std::mutex> lock(g_mu);
    g_pa_cache.clear();
    g_sig_to_pa.clear();
    g_next_synth_pa = 0xACE000000000ull;
}

uint64_t cache_firmware_blob(const std::string& signature,
                             const std::vector<uint8_t>& data,
                             uint64_t preferred_pa) {
    std::lock_guard<std::mutex> lock(g_mu);
    uint64_t pa = preferred_pa;
    if (!pa) {
        auto it = g_sig_to_pa.find(signature);
        if (it != g_sig_to_pa.end()) pa = it->second;
        else {
            pa = g_next_synth_pa;
            g_next_synth_pa += 0x10000ull;
        }
    }
    Blob b;
    b.signature = signature;
    b.data = data;
    g_pa_cache[pa] = std::move(b);
    g_sig_to_pa[signature] = pa;
    return pa;
}

std::vector<uint8_t> cached_firmware_blob(const std::string& signature) {
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_sig_to_pa.find(signature);
    if (it == g_sig_to_pa.end()) return {};
    auto jt = g_pa_cache.find(it->second);
    if (jt == g_pa_cache.end()) return {};
    return jt->second.data;
}

bool read_physical(uint64_t pa, void* buf, size_t size) {
    if (!buf || size == 0) return false;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_hook) {
            if (g_hook(pa, buf, size)) return true;
        }
        if (read_from_maps_locked(pa, buf, size)) return true;
    }
#if LR_PLATFORM_LINUX
    return read_dev_mem(pa, buf, size);
#else
    return false;
#endif
}

Result<std::vector<uint8_t>> read_acpi_table_bytes(const char* signature) {
    if (!signature || std::strlen(signature) != 4)
        return Result<std::vector<uint8_t>>({}, "Invalid ACPI signature (need 4 chars)");

    // Cache hit
    {
        auto cached = cached_firmware_blob(signature);
        if (!cached.empty()) return Result<std::vector<uint8_t>>(std::move(cached));
    }

#if LR_PLATFORM_LINUX
    {
        char path[256];
        std::snprintf(path, sizeof(path), "/sys/firmware/acpi/tables/%.4s", signature);
        auto data = read_file_bytes(path);
        if (!data.empty()) {
            cache_firmware_blob(signature, data);
            return Result<std::vector<uint8_t>>(std::move(data));
        }
        // SSDT may have numeric suffixes SSDT1, SSDT2...
        if (std::memcmp(signature, "SSDT", 4) == 0) {
            for (int i = 1; i < 64; ++i) {
                std::snprintf(path, sizeof(path), "/sys/firmware/acpi/tables/SSDT%d", i);
                data = read_file_bytes(path);
                if (!data.empty()) {
                    cache_firmware_blob(signature, data);
                    return Result<std::vector<uint8_t>>(std::move(data));
                }
            }
        }
    }
    return Result<std::vector<uint8_t>>({},
        std::string("ACPI table ") + signature + " not found in sysfs");
#elif LR_PLATFORM_WINDOWS
    const uint32_t fourcc = sig_to_fourcc(signature);
    DWORD need = GetSystemFirmwareTable('ACPI', fourcc, nullptr, 0);
    if (!need) {
        return Result<std::vector<uint8_t>>({},
            std::string("GetSystemFirmwareTable ACPI ") + signature + " not found");
    }
    std::vector<uint8_t> table(need);
    DWORD got = GetSystemFirmwareTable('ACPI', fourcc, table.data(), need);
    if (!got || got > need) {
        auto err = os_error("GetSystemFirmwareTable");
        return Result<std::vector<uint8_t>>({}, err.error_msg);
    }
    table.resize(got);
    cache_firmware_blob(signature, table);
    return Result<std::vector<uint8_t>>(std::move(table));
#else
    return Result<std::vector<uint8_t>>({}, "ACPI table read unsupported on this platform");
#endif
}

Result<std::vector<std::string>> list_acpi_signatures() {
    std::vector<std::string> out;
#if LR_PLATFORM_LINUX
    DIR* dir = opendir("/sys/firmware/acpi/tables");
    if (!dir) {
        return Result<std::vector<std::string>>({},
            "Cannot open /sys/firmware/acpi/tables");
    }
    while (dirent* ent = readdir(dir)) {
        if (ent->d_name[0] == '.') continue;
        std::string name = ent->d_name;
        // Skip data/ dynamic subdirs
        if (name == "data" || name == "dynamic") continue;
        // Normalize SSDT1 -> SSDT for first four if alphabetic
        if (name.size() >= 4) {
            std::string sig = name.substr(0, 4);
            if (std::find(out.begin(), out.end(), sig) == out.end())
                out.push_back(sig);
            // Also keep full name for distinct tables when unique
            if (name.size() == 4 && std::find(out.begin(), out.end(), name) == out.end())
                out.push_back(name);
        }
    }
    closedir(dir);
    if (out.empty())
        return Result<std::vector<std::string>>({}, "No ACPI tables in sysfs");
    return Result<std::vector<std::string>>(std::move(out));
#elif LR_PLATFORM_WINDOWS
    DWORD need = EnumSystemFirmwareTables('ACPI', nullptr, 0);
    if (!need) {
        return Result<std::vector<std::string>>({},
            "EnumSystemFirmwareTables(ACPI) failed or empty");
    }
    std::vector<uint8_t> buf(need);
    DWORD got = EnumSystemFirmwareTables('ACPI', buf.data(), need);
    if (!got) {
        auto err = os_error("EnumSystemFirmwareTables");
        return Result<std::vector<std::string>>({}, err.error_msg);
    }
    for (DWORD i = 0; i + 4 <= got; i += 4) {
        uint32_t fourcc = 0;
        std::memcpy(&fourcc, buf.data() + i, 4);
        out.push_back(fourcc_to_sig(fourcc));
    }
    if (out.empty())
        return Result<std::vector<std::string>>({}, "No ACPI firmware tables enumerated");
    return Result<std::vector<std::string>>(std::move(out));
#else
    return Result<std::vector<std::string>>({}, "ACPI enumeration unsupported");
#endif
}

bool ensure_firmware_variable_privilege() {
#if LR_PLATFORM_WINDOWS
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }
    LUID luid{};
    if (!LookupPrivilegeValueA(nullptr, "SeSystemEnvironmentPrivilege", &luid)) {
        CloseHandle(token);
        return false;
    }
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    const BOOL ok = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    const DWORD err = GetLastError();
    CloseHandle(token);
    // AdjustTokenPrivileges can return TRUE even when not all privileges assigned
    return ok && err != ERROR_NOT_ALL_ASSIGNED;
#else
    return true;
#endif
}

} // namespace real::uefi
