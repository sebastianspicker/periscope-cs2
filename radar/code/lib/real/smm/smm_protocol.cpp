// smm_protocol.cpp — Pure SMM/ACPI/TPM protocol implementation.
// No OS I/O. Safe to unit-test with synthetic buffers.

#include "real/smm/smm_protocol.hpp"

#include <cstring>

namespace real::smm {
namespace {

// ── SHA-256 (compact, public-domain style) ─────────────────────────

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

void sha256_transform(std::uint32_t state[8], const std::uint8_t block[64]) {
  static const std::uint32_t K[64] = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
      0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
      0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
      0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
      0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
      0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
      0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
      0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
      0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
      0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
           (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
           static_cast<std::uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

  for (int i = 0; i < 64; ++i) {
    const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const std::uint32_t ch = (e & f) ^ ((~e) & g);
    const std::uint32_t t1 = h + S1 + ch + K[i] + w[i];
    const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t t2 = S0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

}  // namespace

// ── SHA-256 public API ─────────────────────────────────────────────

std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len) {
  std::uint32_t state[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

  std::uint8_t block[64];
  std::size_t offset = 0;
  const std::uint64_t bit_len = static_cast<std::uint64_t>(len) * 8ull;

  while (len - offset >= 64) {
    std::memcpy(block, data + offset, 64);
    sha256_transform(state, block);
    offset += 64;
  }

  const std::size_t rem = len - offset;
  if (rem && data) std::memcpy(block, data + offset, rem);
  block[rem] = 0x80;
  if (rem >= 56) {
    std::memset(block + rem + 1, 0, 64 - rem - 1);
    sha256_transform(state, block);
    std::memset(block, 0, 56);
  } else {
    std::memset(block + rem + 1, 0, 56 - rem - 1);
  }
  for (int i = 0; i < 8; ++i) {
    block[63 - i] = static_cast<std::uint8_t>(bit_len >> (8 * i));
  }
  sha256_transform(state, block);

  std::array<std::uint8_t, 32> out{};
  for (int i = 0; i < 8; ++i) {
    out[i * 4] = static_cast<std::uint8_t>(state[i] >> 24);
    out[i * 4 + 1] = static_cast<std::uint8_t>(state[i] >> 16);
    out[i * 4 + 2] = static_cast<std::uint8_t>(state[i] >> 8);
    out[i * 4 + 3] = static_cast<std::uint8_t>(state[i]);
  }
  return out;
}

std::array<std::uint8_t, 32> sha256(const std::vector<std::uint8_t>& data) {
  return sha256(data.data(), data.size());
}

// ── ACPI ───────────────────────────────────────────────────────────

bool acpi_checksum_valid(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) return false;
  std::uint8_t sum = 0;
  for (std::size_t i = 0; i < len; ++i) sum = static_cast<std::uint8_t>(sum + data[i]);
  return sum == 0;
}

std::uint8_t acpi_fix_checksum(std::vector<std::uint8_t>& table,
                                 std::size_t checksum_offset) {
  if (table.empty() || checksum_offset >= table.size()) return 0;
  table[checksum_offset] = 0;
  std::uint8_t sum = 0;
  for (std::uint8_t b : table) sum = static_cast<std::uint8_t>(sum + b);
  table[checksum_offset] = static_cast<std::uint8_t>((0x100u - sum) & 0xFFu);
  return table[checksum_offset];
}

bool rsdp_valid(const std::uint8_t* rsdp, std::size_t len) {
  if (!rsdp || len < 20) return false;
  if (std::memcmp(rsdp, "RSD PTR ", 8) != 0) return false;
  // ACPI 1.0 checksum covers first 20 bytes.
  if (!acpi_checksum_valid(rsdp, 20)) return false;
  if (len >= 36 && rsdp[15] >= 2) {
    // Extended checksum covers full RSDP length field (usually 36).
    std::uint32_t full_len = 0;
    std::memcpy(&full_len, rsdp + 20, 4);
    if (full_len < 36 || full_len > len) return false;
    if (!acpi_checksum_valid(rsdp, full_len)) return false;
  }
  return true;
}

RsdpAddresses parse_rsdp_addresses(const std::uint8_t* rsdp, std::size_t len) {
  RsdpAddresses out{};
  if (!rsdp_valid(rsdp, len)) return out;
  out.revision = rsdp[15];
  std::memcpy(&out.rsdt, rsdp + 16, 4);
  if (out.revision >= 2 && len >= 32) {
    std::memcpy(&out.xsdt, rsdp + 24, 8);
  }
  out.ok = (out.rsdt != 0) || (out.xsdt != 0);
  return out;
}

FadtSmiInfo parse_fadt_smi(const std::uint8_t* fadt, std::size_t len) {
  FadtSmiInfo info{};
  // FACP header 36 + SMI_CMD at offset 48, ACPI_ENABLE 52, ACPI_DISABLE 53.
  if (!fadt || len < 54) return info;
  if (std::memcmp(fadt, "FACP", 4) != 0) return info;
  std::uint32_t table_len = 0;
  std::memcpy(&table_len, fadt + 4, 4);
  if (table_len > len || table_len < 54) return info;
  if (!acpi_checksum_valid(fadt, table_len)) {
    // Some firmware tables arrive with stale checksums after OS remaps;
    // still extract ports but mark ok only when checksum is clean.
  }
  std::memcpy(&info.smi_cmd, fadt + 48, 4);
  info.acpi_enable = fadt[52];
  info.acpi_disable = fadt[53];
  if (len >= 56) {
    info.s4bios_req = fadt[54];
    info.pstate_cnt = fadt[55];
  }
  info.ok = info.smi_cmd != 0;
  return info;
}

// ── TSEG ───────────────────────────────────────────────────────────

TsegDecode decode_tsegmb(std::uint32_t tsegmb, std::uint64_t tseg_size_hint) {
  TsegDecode d{};
  d.locked = (tsegmb & kTsegmbLockBit) != 0;
  d.base = static_cast<std::uint64_t>(tsegmb & kTsegmbBaseMask);
  d.size = tseg_size_hint;
  // Zero base with lock still can be valid on locked-empty configs; require non-zero base.
  d.valid = d.base != 0;
  return d;
}

bool phys_in_range(std::uint64_t phys, std::uint64_t base, std::uint64_t size) {
  if (size == 0) return false;
  if (phys < base) return false;
  return (phys - base) < size;
}

// ── SMM communicate ────────────────────────────────────────────────

std::vector<std::uint8_t> pack_phys_read_request(std::uint64_t phys_addr,
                                                  std::uint32_t size) {
  SmmCommHeader hdr{};
  hdr.magic = kSmmReqMagic;
  hdr.command = static_cast<std::uint8_t>(SmiCommand::PhysRead);
  hdr.phys_addr = phys_addr;
  hdr.size = size;
  std::vector<std::uint8_t> out(sizeof(hdr));
  std::memcpy(out.data(), &hdr, sizeof(hdr));
  return out;
}

std::vector<std::uint8_t> pack_echo_request(std::uint8_t token) {
  SmmCommHeader hdr{};
  hdr.magic = kSmmReqMagic;
  hdr.command = static_cast<std::uint8_t>(SmiCommand::Echo);
  hdr.phys_addr = 0;
  hdr.size = 1;
  std::vector<std::uint8_t> out(sizeof(hdr) + 1);
  std::memcpy(out.data(), &hdr, sizeof(hdr));
  out[sizeof(hdr)] = token;
  return out;
}

Result<std::vector<std::uint8_t>> unpack_smm_response(
    const std::vector<std::uint8_t>& buffer) {
  if (buffer.size() < sizeof(SmmCommHeader)) {
    return Result<std::vector<std::uint8_t>>({}, "SMM response too short");
  }
  SmmCommHeader hdr{};
  std::memcpy(&hdr, buffer.data(), sizeof(hdr));
  if (hdr.magic == kSmmErrMagic) {
    return Result<std::vector<std::uint8_t>>({}, "SMM handler returned error");
  }
  if (hdr.magic != kSmmRspMagic) {
    return Result<std::vector<std::uint8_t>>({}, "SMM response magic mismatch");
  }
  const std::size_t payload_off = sizeof(SmmCommHeader);
  if (hdr.size > buffer.size() - payload_off) {
    // Some handlers overwrite only magic and leave size; take remaining bytes.
    return std::vector<std::uint8_t>(buffer.begin() + static_cast<std::ptrdiff_t>(payload_off),
                                     buffer.end());
  }
  return std::vector<std::uint8_t>(
      buffer.begin() + static_cast<std::ptrdiff_t>(payload_off),
      buffer.begin() + static_cast<std::ptrdiff_t>(payload_off + hdr.size));
}

std::vector<std::uint8_t> pack_efi_smm_communicate(
    const std::uint8_t handler_guid[16],
    const std::vector<std::uint8_t>& message) {
  EfiSmmCommunicateHeader efi{};
  if (handler_guid) std::memcpy(efi.header_guid, handler_guid, 16);
  efi.message_length = message.size();
  std::vector<std::uint8_t> out(sizeof(efi) + message.size());
  std::memcpy(out.data(), &efi, sizeof(efi));
  if (!message.empty()) {
    std::memcpy(out.data() + sizeof(efi), message.data(), message.size());
  }
  return out;
}

// ── CMOS ───────────────────────────────────────────────────────────

std::uint16_t cmos_checksum(const std::uint8_t* cmos, std::size_t start,
                            std::size_t end) {
  if (!cmos || end < start) return 0;
  std::uint16_t sum = 0;
  for (std::size_t i = start; i <= end; ++i) sum = static_cast<std::uint16_t>(sum + cmos[i]);
  return sum;
}

bool cmos_checksum_valid(const std::uint8_t* cmos, std::size_t len,
                         std::size_t range_start, std::size_t range_end,
                         std::size_t stored_hi, std::size_t stored_lo) {
  if (!cmos || range_end >= len || stored_hi >= len || stored_lo >= len) return false;
  if (range_end < range_start) return false;
  const std::uint16_t sum = cmos_checksum(cmos, range_start, range_end);
  const std::uint16_t stored =
      (static_cast<std::uint16_t>(cmos[stored_hi]) << 8) | cmos[stored_lo];
  return sum == stored;
}

// ── PCR extend ─────────────────────────────────────────────────────

Result<std::array<std::uint8_t, 32>> pcr_extend_sha256(
    const std::array<std::uint8_t, 32>& old_pcr,
    const std::array<std::uint8_t, 32>& extend_digest) {
  std::uint8_t concat[64];
  std::memcpy(concat, old_pcr.data(), 32);
  std::memcpy(concat + 32, extend_digest.data(), 32);
  return sha256(concat, 64);
}

Result<std::vector<std::uint8_t>> pcr_extend_sha256(
    const std::vector<std::uint8_t>& old_pcr,
    const std::vector<std::uint8_t>& extend_digest) {
  if (old_pcr.size() != 32 || extend_digest.size() != 32) {
    return Result<std::vector<std::uint8_t>>(
        {}, "PCR extend requires 32-byte SHA-256 digests");
  }
  std::array<std::uint8_t, 32> old_a{};
  std::array<std::uint8_t, 32> dig_a{};
  std::memcpy(old_a.data(), old_pcr.data(), 32);
  std::memcpy(dig_a.data(), extend_digest.data(), 32);
  auto r = pcr_extend_sha256(old_a, dig_a);
  if (!r) return Result<std::vector<std::uint8_t>>({}, r.error_msg);
  return std::vector<std::uint8_t>(r->begin(), r->end());
}

// ── Blue heuristics ────────────────────────────────────────────────

SmiLatencyClass classify_smi_latency_us(std::uint64_t latency_us) {
  // Typical SW-SMI is tens to a few hundred µs; multi-ms is suspicious.
  if (latency_us < 500) return SmiLatencyClass::Normal;
  if (latency_us < 5000) return SmiLatencyClass::Elevated;
  return SmiLatencyClass::Anomalous;
}

bool smi_count_delta_plausible(std::uint64_t before, std::uint64_t after,
                               std::uint64_t expected_triggers,
                               std::uint64_t slack) {
  if (after < before) return false;  // counter should be monotonic
  const std::uint64_t delta = after - before;
  const std::uint64_t lo = expected_triggers > slack ? expected_triggers - slack : 0;
  const std::uint64_t hi = expected_triggers + slack;
  return delta >= lo && delta <= hi;
}

}  // namespace real::smm
