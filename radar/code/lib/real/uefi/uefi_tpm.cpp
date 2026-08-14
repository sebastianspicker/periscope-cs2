// uefi_tpm.cpp — Intel TXT and TPM event log access implementation.

#include "real/uefi/uefi_tpm.hpp"
#include "real/uefi/uefi_fw.hpp"
#include "real/uefi/uefi_phys.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sstream>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#elif LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  if defined(_MSC_VER)
#    include <intrin.h>
#  endif
#endif

namespace real::uefi {
namespace {

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

void cpuid_leaf1(int out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
#if defined(_MSC_VER)
    __cpuidex(out, 1, 0);
#elif defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("cpuid"
                     : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
                     : "a"(1), "c"(0));
#endif
}

std::string event_type_name(uint32_t t) {
    switch (t) {
    case 0x00000000: return "EV_PREBOOT_CERT";
    case 0x00000001: return "EV_POST_CODE";
    case 0x00000003: return "EV_NO_ACTION";
    case 0x00000004: return "EV_SEPARATOR";
    case 0x00000005: return "EV_ACTION";
    case 0x00000006: return "EV_EVENT_TAG";
    case 0x00000007: return "EV_S_CRTM_CONTENTS";
    case 0x00000008: return "EV_S_CRTM_VERSION";
    case 0x80000001: return "EV_EFI_VARIABLE_DRIVER_CONFIG";
    case 0x80000002: return "EV_EFI_VARIABLE_BOOT";
    case 0x80000003: return "EV_EFI_BOOT_SERVICES_APPLICATION";
    case 0x80000004: return "EV_EFI_BOOT_SERVICES_DRIVER";
    case 0x80000005: return "EV_EFI_RUNTIME_SERVICES_DRIVER";
    case 0x80000006: return "EV_EFI_GPT_EVENT";
    case 0x80000007: return "EV_EFI_ACTION";
    case 0x80000008: return "EV_EFI_PLATFORM_FIRMWARE_BLOB";
    case 0x80000009: return "EV_EFI_HANDOFF_TABLES";
    default: {
        char b[32];
        std::snprintf(b, sizeof(b), "EV_0x%08X", t);
        return b;
    }
    }
}

} // namespace

size_t tpm_alg_digest_size(uint16_t alg_id) noexcept {
    switch (alg_id) {
    case 0x0004: return 20; // TPM_ALG_SHA1
    case 0x000B: return 32; // TPM_ALG_SHA256
    case 0x000C: return 48; // TPM_ALG_SHA384
    case 0x000D: return 64; // TPM_ALG_SHA512
    case 0x0012: return 32; // TPM_ALG_SM3_256
    default: return 0;
    }
}

Result<std::vector<TpmEventLogEntry>> parse_tpm_event_log(const uint8_t* data,
                                                          size_t size) {
    if (!data || size < 8)
        return Result<std::vector<TpmEventLogEntry>>({}, "TPM event log buffer too small");

    std::vector<TpmEventLogEntry> events;
    size_t pos = 0;

    // Detect Crypto Agile (TCG_PCR_EVENT2) vs legacy SHA-1 log.
    // Spec ID event: first record is often TCG_PCR_EVENT with EventType EV_NO_ACTION
    // and "Spec ID Event03" in event data — after that, EVENT2 format follows.
    bool crypto_agile = false;

    // Heuristic: if bytes at offset 8 look like a SHA-1 digest region of 20
    // and event size at 28 is plausible → legacy. If digest_count at +8 is small
    // (1..8) after a NO_ACTION Spec ID, switch to agile.
    auto parse_legacy = [&](size_t& p) -> bool {
        // TCG_PCR_EVENT: PCRIndex(4) EventType(4) Digest(20) EventSize(4) Event[EventSize]
        if (p + 32 > size) return false;
        uint32_t pcr = 0, etype = 0, esize = 0;
        std::memcpy(&pcr, data + p, 4);
        std::memcpy(&etype, data + p + 4, 4);
        std::memcpy(&esize, data + p + 28, 4);
        if (esize > 0x100000 || p + 32 + esize > size) return false;
        TpmEventLogEntry e;
        e.pcr_index = pcr;
        e.event_type = etype;
        e.digest_count = 1;
        e.digest.assign(data + p + 8, data + p + 28);
        e.event_data.assign(data + p + 32, data + p + 32 + esize);
        e.description = event_type_name(etype);
        if (esize > 0) {
            // Prefer printable prefix
            size_t n = std::min<size_t>(esize, 64);
            std::string s(reinterpret_cast<const char*>(data + p + 32), n);
            if (s.find("Spec ID Event") != std::string::npos) {
                e.description = s.c_str(); // trim at first NUL naturally via c_str if embedded
                // strip non-print
                for (char& c : s) if (c < 32 || c > 126) c = 0;
                e.description = s.c_str();
                crypto_agile = true;
            }
        }
        events.push_back(std::move(e));
        p += 32 + esize;
        return true;
    };

    auto parse_event2 = [&](size_t& p) -> bool {
        // TCG_PCR_EVENT2: PCRIndex(4) EventType(4) Digests TPML_DIGEST_VALUES
        //   count(4) then count * (alg_id(2) + digest[alg_size])
        // then EventSize(4) Event[EventSize]
        if (p + 12 > size) return false;
        uint32_t pcr = 0, etype = 0, dig_count = 0;
        std::memcpy(&pcr, data + p, 4);
        std::memcpy(&etype, data + p + 4, 4);
        std::memcpy(&dig_count, data + p + 8, 4);
        if (dig_count == 0 || dig_count > 16) return false;
        size_t q = p + 12;
        TpmEventLogEntry e;
        e.pcr_index = pcr;
        e.event_type = etype;
        e.digest_count = dig_count;
        for (uint32_t d = 0; d < dig_count; ++d) {
            if (q + 2 > size) return false;
            uint16_t alg = 0;
            std::memcpy(&alg, data + q, 2);
            q += 2;
            size_t dsz = tpm_alg_digest_size(alg);
            if (dsz == 0 || q + dsz > size) return false;
            if (e.digest.empty())
                e.digest.assign(data + q, data + q + dsz);
            q += dsz;
        }
        if (q + 4 > size) return false;
        uint32_t esize = 0;
        std::memcpy(&esize, data + q, 4);
        q += 4;
        if (esize > 0x100000 || q + esize > size) return false;
        e.event_data.assign(data + q, data + q + esize);
        e.description = event_type_name(etype);
        if (esize > 0) {
            size_t n = std::min<size_t>(esize, 64);
            std::string s(reinterpret_cast<const char*>(data + q), n);
            for (char& c : s) if (static_cast<unsigned char>(c) < 32 ||
                                  static_cast<unsigned char>(c) > 126) { c = '\0'; break; }
            if (!s.empty() && s[0]) e.description = s.c_str();
        }
        events.push_back(std::move(e));
        p = q + esize;
        return true;
    };

    // First record is almost always legacy TCG_PCR_EVENT (Spec ID or SHA1 log)
    if (!parse_legacy(pos)) {
        // Try pure EVENT2 stream
        pos = 0;
        crypto_agile = true;
        while (pos < size) {
            size_t before = pos;
            if (!parse_event2(pos) || pos == before) break;
        }
        if (events.empty())
            return Result<std::vector<TpmEventLogEntry>>({},
                "Failed to parse TPM event log (no valid records)");
        return Result<std::vector<TpmEventLogEntry>>(std::move(events));
    }

    while (pos < size) {
        size_t before = pos;
        bool ok = crypto_agile ? parse_event2(pos) : parse_legacy(pos);
        if (!ok) {
            // Try alternate format once
            if (!crypto_agile) {
                crypto_agile = true;
                ok = parse_event2(pos);
            }
            if (!ok || pos == before) break;
        }
        if (pos == before) break;
    }

    if (events.empty())
        return Result<std::vector<TpmEventLogEntry>>({}, "TPM event log contained no events");
    return Result<std::vector<TpmEventLogEntry>>(std::move(events));
}

Result<IntelTxtData> parse_txt_heap(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(IntelTxtHeap))
        return Result<IntelTxtData>({}, "TXT heap buffer too small");
    IntelTxtHeap heap{};
    std::memcpy(&heap, data, sizeof(heap));
    if (heap.size < sizeof(IntelTxtHeap) || heap.size > size)
        return Result<IntelTxtData>({}, "TXT heap size field invalid");

    IntelTxtData out;
    out.heap_size = heap.size;
    out.txt_capable = true;

    size_t pos = sizeof(IntelTxtHeap);
    const size_t end = static_cast<size_t>(heap.size);
    uint64_t walked = 0;
    // Prefer entry_count if sane; else walk by entry.size
    const uint64_t max_entries = (heap.entry_count > 0 && heap.entry_count < 1024)
                                     ? heap.entry_count : 1024;
    while (pos + sizeof(IntelTxtHeapEntry) <= end && walked < max_entries) {
        IntelTxtHeapEntry ent{};
        std::memcpy(&ent, data + pos, sizeof(ent));
        if (ent.size < sizeof(IntelTxtHeapEntry) || pos + ent.size > end)
            break;
        if (ent.type == kTxtHeapBiosSpecVer) {
            if (ent.size >= sizeof(IntelTxtHeapEntry) + 4) {
                std::memcpy(&out.bios_spec_ver, data + pos + sizeof(IntelTxtHeapEntry), 4);
            } else {
                out.bios_spec_ver = 1;
            }
        } else if (ent.type == kTxtHeapAcpiData) {
            out.acpi_data_count++;
        }
        pos += static_cast<size_t>(ent.size);
        ++walked;
    }
    std::ostringstream oss;
    oss << "TXT heap size=" << out.heap_size
        << " bios_spec=" << out.bios_spec_ver
        << " acpi_entries=" << out.acpi_data_count;
    out.detail = oss.str();
    return Result<IntelTxtData>(std::move(out));
}

Result<std::vector<TpmEventLogEntry>> read_tpm_event_log() {
    // 1) Linux binary event log
#if LR_PLATFORM_LINUX
    {
        // Common paths for TPM 2.0 binary event log
        const char* paths[] = {
            "/sys/kernel/security/tpm0/binary_bios_measurements",
            "/sys/kernel/security/tpm0/binary_bios_measurements",
        };
        for (const char* path : paths) {
            auto bytes = read_file_bytes(path);
            if (bytes.size() >= 32) {
                auto parsed = parse_tpm_event_log(bytes.data(), bytes.size());
                if (parsed) {
                    std::printf("[uefi:txt] TPM event log from %s: %zu entries\n",
                                path, parsed->size());
                    return parsed;
                }
            }
        }
        // ASCII ascii_bios_measurements as last resort — not binary, skip
    }
#endif

    // 2) ACPI TPM2 table → log address (needs phys read)
    {
        auto tpm2 = read_acpi_table_bytes("TPM2");
        if (tpm2 && tpm2->size() >= 52) {
            // ACPI TPM2: header(36) + platform_class(2) + reserved(2) +
            // address_of_control_area(8) + start_method(4) + ...
            // Event log may be in separate TCPA table.
        }
        auto tcpa = read_acpi_table_bytes("TCPA");
        if (tcpa && tcpa->size() >= 50) {
            // TCPA: after header, platform class, then log min length + log start
            // Layout varies; TCG ACPI: LogMaxLength at +36+4, LogStart at +36+8 for client
            if (tcpa->size() >= 52) {
                uint32_t log_len = 0;
                uint64_t log_pa = 0;
                std::memcpy(&log_len, tcpa->data() + 36 + 4, 4);
                std::memcpy(&log_pa, tcpa->data() + 36 + 8, 8);
                if (log_pa && log_len > 32 && log_len < 8u * 1024u * 1024u) {
                    std::vector<uint8_t> log(log_len);
                    if (read_physical(log_pa, log.data(), log.size())) {
                        auto parsed = parse_tpm_event_log(log.data(), log.size());
                        if (parsed) {
                            std::printf("[uefi:txt] TPM event log from TCPA: %zu entries\n",
                                        parsed->size());
                            return parsed;
                        }
                    }
                }
            }
        }
    }

    // 3) EFI config table path
    auto rsdp = find_rsdp_pa();
    if (rsdp) {
        auto st = read_efi_system_table(*rsdp);
        if (st) {
            auto tpm_table_pa = find_config_table(*st, guid::EfiTpmTableHi, guid::EfiTpmTableLo);
            if (tpm_table_pa && *tpm_table_pa) {
                struct Tpm2Config {
                    uint8_t  platform_class[2];
                    uint8_t  reserved[2];
                    uint64_t address_of_control_area;
                    uint32_t start_method;
                };
                // Some firmwares put event log pointer after start_method
                uint8_t hdr[64]{};
                if (read_physical(*tpm_table_pa, hdr, sizeof(hdr))) {
                    uint64_t log_pa = 0;
                    uint32_t log_len = 0;
                    std::memcpy(&log_len, hdr + 8, 4);
                    std::memcpy(&log_pa, hdr + 12, 8);
                    if (log_pa && log_len > 32 && log_len < 8u * 1024u * 1024u) {
                        std::vector<uint8_t> log(log_len);
                        if (read_physical(log_pa, log.data(), log.size())) {
                            auto parsed = parse_tpm_event_log(log.data(), log.size());
                            if (parsed) return parsed;
                        }
                    }
                }
            }
        }
    }

    return Result<std::vector<TpmEventLogEntry>>({},
        "TPM event log not available (no binary log, TCPA, or config table access)");
}

Result<IntelTxtData> read_intel_txt_data() {
    IntelTxtData data;

    int cpu_info[4] = {};
    cpuid_leaf1(cpu_info);
    data.smx_enabled = (cpu_info[2] & (1 << 6)) != 0;  // CPUID.1:ECX[6] SMX
    const bool vmx = (cpu_info[2] & (1 << 5)) != 0;
    data.txt_capable = data.smx_enabled && vmx;

    // TXT public config space
    uint32_t sts = 0;
    bool read_sts = read_physical(kTxtPublicBase + 0x000, &sts, sizeof(sts));
    if (!read_sts) {
        // Try alternate STS offset used by some docs (TXT.STS at 0x000 of private;
        // public space 0xFED30000 maps TXT.VER.FSBIF etc.)
        read_sts = read_physical(kTxtPublicBase + 0x010, &sts, sizeof(sts));
    }
    if (read_sts) {
        // SENTER.DONE.STS is bit 0 of TXT.ESTS at private+0x008; public probes vary.
        data.txt_enabled = (sts != 0 && sts != 0xFFFFFFFFu);
        data.measured_launch_occurred = (sts & 0x01) != 0;
    }

    uint64_t heap_base = 0, heap_size = 0;
    if (read_physical(kTxtPublicBase + 0x300, &heap_base, sizeof(heap_base))) {
        data.heap_base = heap_base;
    }
    if (read_physical(kTxtPublicBase + 0x308, &heap_size, sizeof(heap_size))) {
        data.heap_size = heap_size;
    }

    if (data.heap_base && data.heap_size && data.heap_size < 16u * 1024u * 1024u) {
        std::vector<uint8_t> heap(static_cast<size_t>(
            std::min<uint64_t>(data.heap_size, 0x10000)));
        if (read_physical(data.heap_base, heap.data(), heap.size())) {
            auto parsed = parse_txt_heap(heap.data(), heap.size());
            if (parsed) {
                data.bios_spec_ver = parsed->bios_spec_ver;
                data.acpi_data_count = parsed->acpi_data_count;
                if (!parsed->detail.empty()) data.detail = parsed->detail;
            }
        }
    }

    if (data.detail.empty()) {
        std::ostringstream oss;
        oss << "TXT capable=" << data.txt_capable
            << " smx=" << data.smx_enabled
            << " enabled=" << data.txt_enabled
            << " launch=" << data.measured_launch_occurred
            << " heap=0x" << std::hex << data.heap_base;
        if (!read_sts && !data.heap_base)
            oss << " (public space not readable from usermode)";
        data.detail = oss.str();
    }

    std::printf("[uefi:txt] %s\n", data.detail.c_str());
    return Result<IntelTxtData>(std::move(data));
}

Result<bool> txt_platform_supported() {
    auto d = read_intel_txt_data();
    if (!d) return Result<bool>(false, d.error_msg);
    return Result<bool>(d->txt_capable);
}

Result<bool> measured_launch_occurred() {
    auto d = read_intel_txt_data();
    if (!d) return Result<bool>(false, d.error_msg);
    return Result<bool>(d->measured_launch_occurred);
}

} // namespace real::uefi
