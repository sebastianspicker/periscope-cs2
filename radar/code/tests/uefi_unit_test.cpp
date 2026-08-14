// uefi_unit_test.cpp — Unit tests for shipped UEFI parsers and helpers.
//
// Drives real::uefi::parse_* / map_* / acpi_checksum_* on constructed fixtures.
// No mocks of the unit under test; no re-implemented oracle.
//
// Build: cmake -DLR_ENABLE_REAL_UEFI=ON ... && cmake --build . --target uefi_unit_test
// Run:   uefi_unit_test

#include "real/uefi/uefi_phys.hpp"
#include "real/uefi/uefi_fw.hpp"
#include "real/uefi/uefi_dma.hpp"
#include "real/uefi/uefi_tpm.hpp"
#include "real/uefi/uefi_var.hpp"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>

static int g_fails = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      std::printf("FAIL: %s\n", msg);                                          \
      ++g_fails;                                                               \
    } else {                                                                   \
      std::printf("ok: %s\n", msg);                                            \
    }                                                                          \
  } while (0)

namespace {

using real::uefi::acpi_checksum_valid;
using real::uefi::acpi_fix_checksum;
using real::uefi::parse_rsdp;
using real::uefi::parse_dmar_table;
using real::uefi::parse_ivrs_table;
using real::uefi::parse_sdt_entry_addresses;
using real::uefi::parse_mcfg_table;
using real::uefi::parse_tpm_event_log;
using real::uefi::parse_efi_memory_map;
using real::uefi::parse_efi_system_table;
using real::uefi::parse_config_table_array;
using real::uefi::map_secure_boot_fields;
using real::uefi::tpm_alg_digest_size;
using real::uefi::kEfiSystemTableSignature;

void fill_sdt_header(std::vector<uint8_t>& buf, const char* sig4, uint32_t length) {
    if (buf.size() < length) buf.resize(length, 0);
    std::memcpy(buf.data(), sig4, 4);
    std::memcpy(buf.data() + 4, &length, 4);
    buf[8] = 1; // revision
    buf[9] = 0; // checksum placeholder
    std::memcpy(buf.data() + 10, "OEMID ", 6);
    std::memcpy(buf.data() + 16, "OEMTABLE", 8);
    acpi_fix_checksum(buf.data(), length);
}

void test_acpi_checksum() {
    std::vector<uint8_t> t(36, 0);
    fill_sdt_header(t, "FACP", 36);
    CHECK(acpi_checksum_valid(t.data(), 36), "fixed FACP checksum validates");
    t[20] ^= 0xFF;
    CHECK(!acpi_checksum_valid(t.data(), 36), "tampered FACP checksum fails");
}

void test_rsdp_checksum() {
    // Build valid ACPI 2.0 RSDP (36 bytes)
    uint8_t rsdp[36]{};
    std::memcpy(rsdp, "RSD PTR ", 8);
    rsdp[15] = 2; // revision
    // oem
    std::memcpy(rsdp + 9, "BOCHS ", 6);
    uint32_t len = 36;
    std::memcpy(rsdp + 20, &len, 4);
    uint64_t xsdt = 0x1000;
    std::memcpy(rsdp + 24, &xsdt, 8);
    // checksum first 20
    rsdp[8] = 0;
    {
        uint8_t sum = 0;
        for (int i = 0; i < 20; ++i) sum = static_cast<uint8_t>(sum + rsdp[i]);
        rsdp[8] = static_cast<uint8_t>((256 - sum) & 0xFF);
    }
    rsdp[32] = 0;
    {
        uint8_t sum = 0;
        for (int i = 0; i < 36; ++i) sum = static_cast<uint8_t>(sum + rsdp[i]);
        rsdp[32] = static_cast<uint8_t>((256 - sum) & 0xFF);
    }

    auto ok = parse_rsdp(rsdp, sizeof(rsdp));
    CHECK(ok, "valid RSDP parses");
    if (ok) {
        CHECK(ok->revision == 2, "RSDP rev 2");
        CHECK(ok->xsdt_address == 0x1000, "RSDP xsdt address");
    }

    uint8_t bad[36];
    std::memcpy(bad, rsdp, 36);
    bad[8] ^= 0x01;
    auto fail = parse_rsdp(bad, sizeof(bad));
    CHECK(!fail, "RSDP with bad checksum rejected");
    CHECK(fail.error_msg.c_str()[0] != 0, "RSDP fail has non-empty error");

    auto tiny = parse_rsdp(rsdp, 10);
    CHECK(!tiny, "undersized RSDP rejected");
}

void test_sdt_walk() {
    // XSDT with 2 entries (64-bit)
    const uint32_t length = 36 + 16;
    std::vector<uint8_t> xsdt(length, 0);
    fill_sdt_header(xsdt, "XSDT", length);
    uint64_t e0 = 0xAAAA0000ull, e1 = 0xBBBB0000ull;
    std::memcpy(xsdt.data() + 36, &e0, 8);
    std::memcpy(xsdt.data() + 44, &e1, 8);
    acpi_fix_checksum(xsdt.data(), length);

    auto addrs = parse_sdt_entry_addresses(xsdt.data(), xsdt.size(), true);
    CHECK(addrs, "XSDT parse ok");
    if (addrs) {
        CHECK(addrs->size() == 2, "XSDT two entries");
        CHECK((*addrs)[0] == 0xAAAA0000ull && (*addrs)[1] == 0xBBBB0000ull,
              "XSDT entry PAs match");
    }

    // Bad checksum
    xsdt[10] ^= 0x55;
    auto bad = parse_sdt_entry_addresses(xsdt.data(), xsdt.size(), true);
    CHECK(!bad, "XSDT bad checksum rejected");
}

void test_dmar_parse() {
    // DMAR header + one DRHD (type 0, length 16) with INCLUDE_PCI_ALL
    const uint32_t length = 48 + 16; // AcpiDmarHeader is 48 bytes (36+12)
    std::vector<uint8_t> dmar(length, 0);
    fill_sdt_header(dmar, "DMAR", length);
    dmar[36] = 39; // host address width
    dmar[37] = 0;  // flags
    // DRHD at offset 48
    uint16_t type = 0, dlen = 16;
    std::memcpy(dmar.data() + 48, &type, 2);
    std::memcpy(dmar.data() + 50, &dlen, 2);
    dmar[52] = 0x01; // INCLUDE_PCI_ALL
    uint64_t base = 0xFED90000ull;
    std::memcpy(dmar.data() + 56, &base, 8);
    acpi_fix_checksum(dmar.data(), length);

    auto info = parse_dmar_table(dmar.data(), dmar.size());
    CHECK(info, "DMAR parse ok");
    if (info) {
        CHECK(info->drhd_count == 1, "DMAR one DRHD");
        CHECK(info->dma_remap_active, "DMAR remap active when DRHD present");
        CHECK(info->include_all_pci, "DMAR INCLUDE_PCI_ALL");
        CHECK(info->base_address == 0xFED90000ull, "DMAR DRHD base");
        CHECK(info->host_address_width == 39, "DMAR host width");
        CHECK(!info->detail.empty(), "DMAR detail non-empty");
    }

    // Truncated DRHD length overrun
    std::vector<uint8_t> bad = dmar;
    uint16_t bad_len = 200;
    std::memcpy(bad.data() + 50, &bad_len, 2);
    acpi_fix_checksum(bad.data(), length);
    auto fail = parse_dmar_table(bad.data(), bad.size());
    CHECK(!fail, "DMAR structure overrun rejected");

    // Wrong signature
    bad = dmar;
    bad[0] = 'X';
    acpi_fix_checksum(bad.data(), length);
    CHECK(!parse_dmar_table(bad.data(), bad.size()), "DMAR bad signature rejected");
}

void test_ivrs_parse() {
    // IVRS header (48 bytes) + IVHD type 0x10 length 24
    const uint32_t length = 48 + 24;
    std::vector<uint8_t> ivrs(length, 0);
    fill_sdt_header(ivrs, "IVRS", length);
    uint32_t iv_info = 0xE0; // HAW bits etc.
    std::memcpy(ivrs.data() + 36, &iv_info, 4);
    ivrs[48] = 0x10; // type
    uint16_t ilen = 24;
    std::memcpy(ivrs.data() + 50, &ilen, 2);
    uint64_t base = 0xFEC00000ull;
    std::memcpy(ivrs.data() + 56, &base, 8);
    acpi_fix_checksum(ivrs.data(), length);

    auto info = parse_ivrs_table(ivrs.data(), ivrs.size());
    CHECK(info, "IVRS parse ok");
    if (info) {
        CHECK(info->drhd_count == 1, "IVRS one IVHD");
        CHECK(info->dma_remap_active, "IVRS active");
        CHECK(info->base_address == 0xFEC00000ull, "IVRS IVHD base");
        CHECK(!info->detail.empty(), "IVRS detail non-empty");
    }
}

void test_mcfg_parse() {
    const uint32_t length = 44 + 16;
    std::vector<uint8_t> mcfg(length, 0);
    fill_sdt_header(mcfg, "MCFG", length);
    // reserved 8 bytes at 36..43 already zero
    uint64_t base = 0xE0000000ull;
    std::memcpy(mcfg.data() + 44, &base, 8);
    uint16_t seg = 0;
    std::memcpy(mcfg.data() + 52, &seg, 2);
    mcfg[54] = 0;   // start bus
    mcfg[55] = 0xFF; // end bus
    acpi_fix_checksum(mcfg.data(), length);

    auto info = parse_mcfg_table(mcfg.data(), mcfg.size());
    CHECK(info, "MCFG parse ok");
    if (info) {
        CHECK(info->base_address == 0xE0000000ull, "MCFG base");
        CHECK(info->start_bus == 0 && info->end_bus == 0xFF, "MCFG bus range");
    }

    mcfg[9] ^= 1; // break checksum without recompute
    CHECK(!parse_mcfg_table(mcfg.data(), mcfg.size()), "MCFG bad checksum rejected");
}

void test_tpm_event_log() {
    // Legacy SHA-1 first event (Spec ID) + one EVENT2 record
    std::vector<uint8_t> log;

    auto append_legacy = [&](uint32_t pcr, uint32_t etype, const std::vector<uint8_t>& dig20,
                             const std::string& ev) {
        // dig20 must be 20 bytes
        std::vector<uint8_t> d = dig20;
        d.resize(20, 0);
        uint32_t esize = static_cast<uint32_t>(ev.size());
        size_t off = log.size();
        log.resize(off + 32 + esize);
        std::memcpy(log.data() + off, &pcr, 4);
        std::memcpy(log.data() + off + 4, &etype, 4);
        std::memcpy(log.data() + off + 8, d.data(), 20);
        std::memcpy(log.data() + off + 28, &esize, 4);
        std::memcpy(log.data() + off + 32, ev.data(), esize);
    };

    append_legacy(0, 0x00000003 /*EV_NO_ACTION*/, std::vector<uint8_t>(20, 0),
                  std::string("Spec ID Event03\0", 16));

    // EVENT2: PCR0, EV_SEPARATOR, one SHA256 digest
    {
        uint32_t pcr = 0, etype = 0x00000004, dig_count = 1;
        uint16_t alg = 0x000B; // SHA256
        std::vector<uint8_t> dig(32, 0xAB);
        uint32_t esize = 4;
        uint8_t edata[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        size_t need = 4 + 4 + 4 + 2 + 32 + 4 + 4;
        size_t off = log.size();
        log.resize(off + need);
        std::memcpy(log.data() + off, &pcr, 4);
        std::memcpy(log.data() + off + 4, &etype, 4);
        std::memcpy(log.data() + off + 8, &dig_count, 4);
        std::memcpy(log.data() + off + 12, &alg, 2);
        std::memcpy(log.data() + off + 14, dig.data(), 32);
        std::memcpy(log.data() + off + 46, &esize, 4);
        std::memcpy(log.data() + off + 50, edata, 4);
    }

    auto events = parse_tpm_event_log(log.data(), log.size());
    CHECK(events, "TPM event log parse ok");
    if (events) {
        CHECK(events->size() >= 2, "TPM log has >=2 events");
        CHECK((*events)[0].event_type == 0x3, "first event EV_NO_ACTION");
        CHECK((*events)[1].digest.size() == 32, "EVENT2 SHA256 digest size");
        CHECK((*events)[1].digest_count == 1, "EVENT2 digest_count");
        CHECK((*events)[1].event_type == 0x4, "second event separator");
    }

    // Truncated EVENT2 digest walk should fail or stop without success-empty
    std::vector<uint8_t> truncated = log;
    truncated.resize(log.size() - 10);
    // Still may parse first event — ensure we don't return success with zero after total garbage
    std::vector<uint8_t> garbage(16, 0xEE);
    auto g = parse_tpm_event_log(garbage.data(), garbage.size());
    CHECK(!g, "garbage TPM log rejected");

    CHECK(tpm_alg_digest_size(0x000B) == 32, "SHA256 digest size helper");
    CHECK(tpm_alg_digest_size(0x0004) == 20, "SHA1 digest size helper");
    CHECK(tpm_alg_digest_size(0xFFFF) == 0, "unknown alg size 0");
}

void test_efi_system_table_and_config() {
    real::uefi::EfiSystemTable st{};
    st.header.signature = kEfiSystemTableSignature;
    st.header.revision = 0x0002001E;
    st.header.header_size = static_cast<uint32_t>(sizeof(real::uefi::EfiTableHeader));
    st.number_of_table_entries = 2;
    st.configuration_table = 0x2000;
    st.runtime_services = 0x3000;

    auto parsed = parse_efi_system_table(reinterpret_cast<const uint8_t*>(&st), sizeof(st));
    CHECK(parsed, "EFI system table parse ok");
    if (parsed) {
        CHECK(parsed->runtime_services == 0x3000, "EFI ST runtime services");
        CHECK(parsed->number_of_table_entries == 2, "EFI ST entry count");
    }

    real::uefi::EfiSystemTable bad = st;
    bad.header.signature = 0;
    CHECK(!parse_efi_system_table(reinterpret_cast<const uint8_t*>(&bad), sizeof(bad)),
          "EFI ST bad signature rejected");

    // Config table array
    real::uefi::EfiConfigurationTable arr[2]{};
    arr[0].vendor_guid_hi = 0x1111; arr[0].vendor_guid_lo = 0x2222; arr[0].vendor_table = 0xABC;
    arr[1].vendor_guid_hi = 0x3333; arr[1].vendor_guid_lo = 0x4444; arr[1].vendor_table = 0xDEF;
    auto ct = parse_config_table_array(reinterpret_cast<const uint8_t*>(arr),
                                       sizeof(arr), 2);
    CHECK(ct && ct->size() == 2, "config table array parse");
    if (ct) {
        CHECK((*ct)[0].table_pa == 0xABC && (*ct)[1].table_pa == 0xDEF,
              "config table PAs");
    }
}

void test_memory_map_parse() {
    // Two descriptors, descriptor_size = 48
    constexpr size_t kDesc = 48;
    std::vector<uint8_t> map(kDesc * 2, 0);
    // desc0: Conventional @ 0x100000, 0x100 pages
    uint32_t type0 = 7; // ConventionalMemory
    uint64_t pa0 = 0x100000, pages0 = 0x100, attr0 = 0xF;
    std::memcpy(map.data() + 0, &type0, 4);
    std::memcpy(map.data() + 8, &pa0, 8);
    std::memcpy(map.data() + 24, &pages0, 8);
    std::memcpy(map.data() + 32, &attr0, 8);
    // desc1: MMIO @ 0xF0000000
    uint32_t type1 = 11;
    uint64_t pa1 = 0xF0000000ull, pages1 = 0x10, attr1 = 0x1;
    std::memcpy(map.data() + kDesc + 0, &type1, 4);
    std::memcpy(map.data() + kDesc + 8, &pa1, 8);
    std::memcpy(map.data() + kDesc + 24, &pages1, 8);
    std::memcpy(map.data() + kDesc + 32, &attr1, 8);

    auto regions = parse_efi_memory_map(map.data(), map.size(), kDesc);
    CHECK(regions && regions->size() == 2, "memory map two regions");
    if (regions) {
        CHECK((*regions)[0].is_available, "conventional is available");
        CHECK((*regions)[0].phys_start == 0x100000, "region0 start");
        CHECK((*regions)[1].is_mmio, "mmio region flagged");
    }
    CHECK(!parse_efi_memory_map(map.data(), map.size(), 16),
          "invalid descriptor size rejected");
}

void test_secure_boot_mapping() {
    auto s = map_secure_boot_fields(1, 0, 0, 1,
                                    {0x01, 0x02}, {0x03}, {0x04, 0x05, 0x06}, {0x07});
    CHECK(s.secure_boot_enabled, "SB enabled mapping");
    CHECK(!s.setup_mode, "setup mode off");
    CHECK(s.deployed_mode, "deployed mode on");
    CHECK(s.pk.size() == 2 && s.dbx.size() == 1, "PK/dbx sizes mapped");

    auto s2 = map_secure_boot_fields(0, 1, 1, 0, {}, {}, {}, {});
    CHECK(!s2.secure_boot_enabled && s2.setup_mode && s2.audit_mode,
          "setup+audit mapping");
}

void test_guid_helpers() {
    uint64_t h0 = 0, h1 = 0;
    real::uefi::efi_global_variable_guid(h0, h1);
    auto s = real::uefi::efi_guid_halves_to_string(h0, h1);
    CHECK(s == "8BE4DF61-93CA-11D2-AA0D-00E098032B8C",
          "EFI Global Variable GUID string");
    auto w = real::uefi::efi_guid_halves_to_windows_string(h0, h1);
    CHECK(w.front() == '{' && w.back() == '}', "Windows GUID braces");

    std::string name;
    uint64_t ph0 = 0, ph1 = 0;
    bool ok = real::uefi::parse_efivar_filename(
        "SecureBoot-8BE4DF61-93CA-11D2-AA0D-00E098032B8C", name, ph0, ph1);
    CHECK(ok && name == "SecureBoot", "efivar filename parse name");
    CHECK(ph0 == h0 && ph1 == h1, "efivar filename parse guid halves");
}

void test_phys_inject_roundtrip() {
    real::uefi::clear_phys_injects();
    std::vector<uint8_t> blob = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
    real::uefi::inject_phys_region(0x9000, blob);
    uint8_t out[4]{};
    CHECK(real::uefi::read_physical(0x9000, out, 4), "phys inject read");
    CHECK(out[0] == 0xDE && out[3] == 0xEF, "phys inject bytes");
    // Sub-range
    uint8_t mid[2]{};
    CHECK(real::uefi::read_physical(0x9002, mid, 2) && mid[0] == 0xBE && mid[1] == 0xEF,
          "phys inject subrange");
    real::uefi::clear_phys_injects();
    CHECK(!real::uefi::read_physical(0x9000, out, 4), "phys inject cleared");
}

} // namespace

int main() {
    std::printf("=== uefi_unit_test ===\n");
    test_acpi_checksum();
    test_rsdp_checksum();
    test_sdt_walk();
    test_dmar_parse();
    test_ivrs_parse();
    test_mcfg_parse();
    test_tpm_event_log();
    test_efi_system_table_and_config();
    test_memory_map_parse();
    test_secure_boot_mapping();
    test_guid_helpers();
    test_phys_inject_roundtrip();

    std::printf("\n%d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
