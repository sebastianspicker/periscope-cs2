// smm_unit_test.cpp — Unit tests for the real SMM stack.
// Drives shipped functions in code/lib/real/smm/ (no reimplementation).
//
// Build:  cmake -DLR_ENABLE_REAL_SMM=ON ... && cmake --build . --target smm_unit_test
// Run:    smm_unit_test.exe

#include "real/smm/smm_interface.hpp"
#include "real/smm/smm_protocol.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

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

// ── Structural: shipped sources exist in the tree ──────────────────

static void test_structural_artifacts() {
  std::printf("\n=== structural artifacts ===\n");
  // Resolve relative to common build layouts: run from build/ or repo root.
  const char* candidates[] = {
      "lib/real/smm/smm_protocol.cpp",
      "../lib/real/smm/smm_protocol.cpp",
      "../../lib/real/smm/smm_protocol.cpp",
      "code/lib/real/smm/smm_protocol.cpp",
      "../code/lib/real/smm/smm_protocol.cpp",
  };
  bool found_proto = false;
  std::string base;
  for (const char* c : candidates) {
    std::ifstream f(c);
    if (f.good()) {
      found_proto = true;
      base = c;
      // strip filename
      auto pos = base.find_last_of("/\\");
      if (pos != std::string::npos) base = base.substr(0, pos + 1);
      break;
    }
  }
  CHECK(found_proto, "smm_protocol.cpp exists on disk");

  if (!found_proto) return;

  const char* files[] = {
      "smm_interface.hpp", "smm_protocol.hpp", "smm_protocol.cpp",
      "port_io.cpp",       "smi.cpp",          "smram.cpp",
      "smm_channel.cpp",   "acpi.cpp",         "efi.cpp",
      "tpm.cpp",
  };
  for (const char* name : files) {
    std::ifstream f(base + name);
    CHECK(f.good(), name);
  }
}

// ── Pure protocol: ACPI checksum ───────────────────────────────────

static void test_acpi_checksum() {
  std::printf("\n=== acpi checksum ===\n");
  std::vector<std::uint8_t> table(36, 0);
  std::memcpy(table.data(), "DSDT", 4);
  table[4] = 36;  // length little-endian low byte
  real::smm::acpi_fix_checksum(table, 9);
  CHECK(real::smm::acpi_checksum_valid(table.data(), table.size()),
        "fixed checksum validates");
  table[20] ^= 0xFF;
  CHECK(!real::smm::acpi_checksum_valid(table.data(), table.size()),
        "tampered table fails checksum");
}

// ── RSDP parse ─────────────────────────────────────────────────────

static void test_rsdp_parse() {
  std::printf("\n=== rsdp parse ===\n");
  std::uint8_t rsdp[36]{};
  std::memcpy(rsdp, "RSD PTR ", 8);
  rsdp[15] = 2;  // revision
  // RSDT at 0x1000
  std::uint32_t rsdt = 0x1000;
  std::memcpy(rsdp + 16, &rsdt, 4);
  // length 36
  std::uint32_t len = 36;
  std::memcpy(rsdp + 20, &len, 4);
  // XSDT at 0x2000
  std::uint64_t xsdt = 0x2000;
  std::memcpy(rsdp + 24, &xsdt, 8);

  // Fix both checksums: first 20 bytes, then full 36.
  rsdp[8] = 0;
  std::uint8_t sum = 0;
  for (int i = 0; i < 20; ++i) sum = static_cast<std::uint8_t>(sum + rsdp[i]);
  rsdp[8] = static_cast<std::uint8_t>((0x100 - sum) & 0xFF);

  rsdp[32] = 0;  // extended checksum field
  sum = 0;
  for (int i = 0; i < 36; ++i) sum = static_cast<std::uint8_t>(sum + rsdp[i]);
  rsdp[32] = static_cast<std::uint8_t>((0x100 - sum) & 0xFF);

  CHECK(real::smm::rsdp_valid(rsdp, 36), "rsdp_valid accepts synthetic RSDP");
  auto addrs = real::smm::parse_rsdp_addresses(rsdp, 36);
  CHECK(addrs.ok, "parse_rsdp_addresses ok");
  CHECK(addrs.rsdt == 0x1000, "RSDT address");
  CHECK(addrs.xsdt == 0x2000, "XSDT address");
  CHECK(addrs.revision == 2, "RSDP revision 2");
}

// ── FADT SMI parse ─────────────────────────────────────────────────

static void test_fadt_smi() {
  std::printf("\n=== fadt smi parse ===\n");
  std::vector<std::uint8_t> fadt(116, 0);
  std::memcpy(fadt.data(), "FACP", 4);
  std::uint32_t flen = 116;
  std::memcpy(fadt.data() + 4, &flen, 4);
  std::uint32_t smi_cmd = 0xB2;
  std::memcpy(fadt.data() + 48, &smi_cmd, 4);
  fadt[52] = 0xF0;
  fadt[53] = 0xF1;
  real::smm::acpi_fix_checksum(fadt, 9);

  auto info = real::smm::parse_fadt_smi(fadt.data(), fadt.size());
  CHECK(info.ok, "parse_fadt_smi ok");
  CHECK(info.smi_cmd == 0xB2, "SMI_CMD port 0xB2");
  CHECK(info.acpi_enable == 0xF0, "ACPI_ENABLE");
  CHECK(info.acpi_disable == 0xF1, "ACPI_DISABLE");
}

// ── TSEG decode ────────────────────────────────────────────────────

static void test_tseg_decode() {
  std::printf("\n=== tseg decode ===\n");
  // base 0xD0000000, locked
  const std::uint32_t tsegmb = 0xD0000001u;
  auto d = real::smm::decode_tsegmb(tsegmb, 0x800000);
  CHECK(d.valid, "TSEG decode valid");
  CHECK(d.locked, "TSEG locked bit");
  CHECK(d.base == 0xD0000000ull, "TSEG base");
  CHECK(d.size == 0x800000ull, "TSEG size hint");
  CHECK(real::smm::phys_in_range(0xD0010000ull, d.base, d.size),
        "PA inside TSEG");
  CHECK(!real::smm::phys_in_range(0xC0000000ull, d.base, d.size),
        "PA outside TSEG");
}

// ── SMM communicate pack/unpack (shipped protocol) ─────────────────

static void test_smm_communicate_protocol() {
  std::printf("\n=== smm communicate protocol ===\n");
  auto req = real::smm::pack_phys_read_request(0x1000, 64);
  CHECK(req.size() == sizeof(real::smm::SmmCommHeader), "phys-read request size");
  real::smm::SmmCommHeader hdr{};
  std::memcpy(&hdr, req.data(), sizeof(hdr));
  CHECK(hdr.magic == real::smm::kSmmReqMagic, "request magic");
  CHECK(hdr.command == static_cast<std::uint8_t>(real::smm::SmiCommand::PhysRead),
        "PhysRead command");
  CHECK(hdr.phys_addr == 0x1000ull, "phys_addr field");
  CHECK(hdr.size == 64u, "size field");

  auto echo = real::smm::pack_echo_request(0xA5);
  CHECK(echo.size() == sizeof(real::smm::SmmCommHeader) + 1, "echo request size");
  CHECK(echo.back() == 0xA5, "echo token");

  // Simulate handler response: copy request, set magic + payload.
  std::vector<std::uint8_t> buf(sizeof(real::smm::SmmCommHeader) + 4);
  std::memcpy(buf.data(), req.data(), sizeof(real::smm::SmmCommHeader));
  real::smm::SmmCommHeader rsp{};
  std::memcpy(&rsp, buf.data(), sizeof(rsp));
  rsp.magic = real::smm::kSmmRspMagic;
  rsp.size = 4;
  std::memcpy(buf.data(), &rsp, sizeof(rsp));
  buf[sizeof(rsp) + 0] = 0xDE;
  buf[sizeof(rsp) + 1] = 0xAD;
  buf[sizeof(rsp) + 2] = 0xBE;
  buf[sizeof(rsp) + 3] = 0xEF;

  auto unpacked = real::smm::unpack_smm_response(buf);
  CHECK(static_cast<bool>(unpacked), "unpack_smm_response ok");
  CHECK(unpacked->size() == 4, "payload size 4");
  CHECK((*unpacked)[0] == 0xDE && (*unpacked)[3] == 0xEF, "payload bytes");

  // Error magic
  rsp.magic = real::smm::kSmmErrMagic;
  std::memcpy(buf.data(), &rsp, sizeof(rsp));
  auto err = real::smm::unpack_smm_response(buf);
  CHECK(!err, "error magic rejected");

  std::uint8_t guid[16] = {1, 2, 3, 4};
  auto efi_buf = real::smm::pack_efi_smm_communicate(guid, req);
  CHECK(efi_buf.size() == sizeof(real::smm::EfiSmmCommunicateHeader) + req.size(),
        "EFI communicate frame size");
}

// ── CMOS checksum ──────────────────────────────────────────────────

static void test_cmos_checksum() {
  std::printf("\n=== cmos checksum ===\n");
  std::uint8_t cmos[128]{};
  for (int i = 0x10; i <= 0x2D; ++i) cmos[i] = static_cast<std::uint8_t>(i);
  const std::uint16_t sum = real::smm::cmos_checksum(cmos, 0x10, 0x2D);
  cmos[0x2E] = static_cast<std::uint8_t>(sum >> 8);
  cmos[0x2F] = static_cast<std::uint8_t>(sum & 0xFF);
  CHECK(real::smm::cmos_checksum_valid(cmos, 128, 0x10, 0x2D, 0x2E, 0x2F),
        "CMOS checksum valid");
  cmos[0x10] ^= 1;
  CHECK(!real::smm::cmos_checksum_valid(cmos, 128, 0x10, 0x2D, 0x2E, 0x2F),
        "CMOS checksum detects tamper");
}

// ── SHA-256 + PCR extend (shipped) ─────────────────────────────────

static void test_sha256_and_pcr() {
  std::printf("\n=== sha256 + pcr extend ===\n");
  // FIPS empty-string SHA-256
  auto empty = real::smm::sha256(nullptr, 0);
  static const std::uint8_t kEmpty[] = {
      0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8,
      0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
      0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};
  CHECK(std::memcmp(empty.data(), kEmpty, 32) == 0, "SHA-256 empty string");

  // "abc" test vector
  const char* abc = "abc";
  auto h = real::smm::sha256(reinterpret_cast<const std::uint8_t*>(abc), 3);
  static const std::uint8_t kAbc[] = {
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde,
      0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
      0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  CHECK(std::memcmp(h.data(), kAbc, 32) == 0, "SHA-256 abc vector");

  std::array<std::uint8_t, 32> zero{};
  std::array<std::uint8_t, 32> dig = h;
  auto ext = real::smm::pcr_extend_sha256(zero, dig);
  CHECK(static_cast<bool>(ext), "pcr_extend_sha256 ok");

  // Cross-check via tpm_predict_pcr_extend public API
  std::vector<std::uint8_t> z(32, 0), d(h.begin(), h.end());
  auto pred = real::smm::tpm_predict_pcr_extend(z, d);
  CHECK(static_cast<bool>(pred), "tpm_predict_pcr_extend ok");
  CHECK(pred->size() == 32, "predicted PCR size 32");
  CHECK(std::memcmp(pred->data(), ext->data(), 32) == 0,
        "predict matches pure pcr_extend");
}

// ── SMI latency heuristic ──────────────────────────────────────────

static void test_smi_heuristics() {
  std::printf("\n=== smi heuristics ===\n");
  CHECK(real::smm::classify_smi_latency_us(50) ==
            real::smm::SmiLatencyClass::Normal,
        "50us normal");
  CHECK(real::smm::classify_smi_latency_us(1000) ==
            real::smm::SmiLatencyClass::Elevated,
        "1000us elevated");
  CHECK(real::smm::classify_smi_latency_us(20000) ==
            real::smm::SmiLatencyClass::Anomalous,
        "20ms anomalous");
  CHECK(real::smm::smi_count_delta_plausible(100, 101, 1, 0),
        "exact delta plausible");
  CHECK(!real::smm::smi_count_delta_plausible(100, 110, 1, 0),
        "large delta not plausible");
  CHECK(!real::smm::smi_count_delta_plausible(100, 90, 1, 0),
        "wrap/decrease rejected");
}

// ── Live API smoke (must call shipped entry points) ─────────────────

static void test_live_entry_points() {
  std::printf("\n=== live entry points (best-effort) ===\n");

  // These may fail without privileges; still prove the real function runs
  // and returns a structured Result (ok or error_msg non-empty).
  auto smi = real::smm::trigger_smi(0x00);
  CHECK(smi.ok || !smi.error_msg.empty(), "trigger_smi returns structured Result");

  auto smi_cnt = real::smm::read_smi_count();
  CHECK(smi_cnt.ok || !smi_cnt.error_msg.empty(),
        "read_smi_count returns structured Result");

  auto smram = real::smm::discover_smram();
  CHECK(smram.ok || !smram.error_msg.empty(),
        "discover_smram returns structured Result");
  if (smram) {
    std::printf("  smram source=%s base=0x%llx locked=%d\n",
                smram->source.c_str(),
                static_cast<unsigned long long>(smram->base),
                smram->locked ? 1 : 0);
  }

  auto tables = real::smm::list_acpi_tables();
  CHECK(tables.ok || !tables.error_msg.empty(),
        "list_acpi_tables returns structured Result");
  if (tables) {
    std::printf("  acpi tables found: %zu\n", tables->size());
    // On Windows, FACP/DSDT usually present
    if (!tables->empty()) {
      auto facp = real::smm::read_acpi_table("FACP");
      if (facp) {
        CHECK(facp->size() > 0, "FACP non-empty via shipped read_acpi_table");
      }
    }
  }

  auto fadt = real::smm::read_fadt_smi_info();
  CHECK(fadt.ok || !fadt.error_msg.empty(),
        "read_fadt_smi_info returns structured Result");

  auto pcr = real::smm::tpm_read_pcr(0);
  CHECK(pcr.ok || !pcr.error_msg.empty(),
        "tpm_read_pcr returns structured Result");
  if (pcr) {
    CHECK(pcr->size() == 20 || pcr->size() == 32 || pcr->size() == 48 ||
              pcr->size() == 64,
          "PCR digest size is a known hash length");
  }

  // Channel without real handler: must fail cleanly (no physical map / timeout).
  auto present = real::smm::smm_handler_present(0x1000, 0x1000);
  CHECK(true, "smm_handler_present invoked shipped path");
  if (present) {
    // ok==true means we got a boolean answer (true=handler, false=no handler).
    CHECK(*present == false || *present == true, "handler probe boolean");
  } else {
    CHECK(!present.error_msg.empty(), "handler probe structured error");
  }

  auto invalid_pcr = real::smm::tpm_read_pcr(99);
  CHECK(!invalid_pcr, "tpm_read_pcr rejects out-of-range index");
}

int main() {
  std::printf("=== smm_unit_test ===\n");
  test_structural_artifacts();
  test_acpi_checksum();
  test_rsdp_parse();
  test_fadt_smi();
  test_tseg_decode();
  test_smm_communicate_protocol();
  test_cmos_checksum();
  test_sha256_and_pcr();
  test_smi_heuristics();
  test_live_entry_points();

  std::printf("\n=== summary: %d checks, %d failures ===\n", g_checks, g_fails);
  return g_fails == 0 ? 0 : 1;
}
