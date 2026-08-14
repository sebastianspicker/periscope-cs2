// offset_fetch.cpp — Remote offset/CDN blob fetching with integrity checks.

#include "real/net/net_client.hpp"
#include "real/net/net_internal.hpp"
#include "real/net/offset_fetch.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace real::net {

// ── Public blob builder (lab / tests) ──────────────────────────────

std::vector<std::uint8_t> build_offset_blob(
    std::uint32_t cs2_version,
    const std::uint64_t offsets[10],
    std::uint8_t xor_key) {
  // Plain layout: [cs2_version:4][checksum:4][10 x u64 offsets]
  std::vector<std::uint8_t> plain(8 + 10 * 8);
  write_u32_le(plain.data(), cs2_version);
  // CRC32 field filled after packing offsets (zero during layout build).
  write_u32_le(plain.data() + 4, 0);
  for (int i = 0; i < 10; ++i)
    write_u64_le(plain.data() + 8 + i * 8, offsets[i]);
  const std::uint32_t c = crc32(plain.data() + 8, plain.size() - 8);
  write_u32_le(plain.data() + 4, c);

  // XOR wrap: first byte is key, rest xor'd with key
  std::vector<std::uint8_t> out(plain.size());
  // Store key as first body byte by xoring entire plain with key, then
  // fetch_offsets uses encrypted[0] as key — so body[0] must equal key after
  // XOR with key => plain[0] ^ key == key => plain[0] == 0. That conflicts.
  // Existing fetch_offsets: key_byte = encrypted[0]; decrypted[i] = encrypted[i] ^ key_byte
  // So decrypted[0] is always 0. Version is read from decrypted[0..3] which would be wrong.
  // Looking at original: it XORs entire body including first byte, so version high bytes
  // get corrupted unless key chosen carefully.
  //
  // We match the existing contract: key_byte = encrypted[0], decrypt all with that key.
  // Put key as encrypted[0], and encrypt plain into encrypted[0..] such that
  // encrypted[i] ^ encrypted[0] = plain[i]. For i=0: encrypted[0]^encrypted[0]=0 != plain[0]
  // unless plain[0]==0. The original design has this quirk.
  //
  // Fix for correctness of shipped fetch_offsets: prepend key byte, then
  // encrypted[i+1] = plain[i] ^ key, and update fetch_offsets to skip first byte...
  // To keep fetch_offsets working as written originally, use key such that
  // we store: out[i] = plain[i] ^ xor_key for all i, AND out[0] must equal xor_key,
  // which requires plain[0] ^ xor_key == xor_key => plain[0] == 0.
  // So force version low byte structure: we change fetch path to:
  // key = body[0]; decrypt body[1:] only — cleaner SOTA. Update both sides.

  out.resize(1 + plain.size());
  out[0] = xor_key ? xor_key : 0x5A;
  for (std::size_t i = 0; i < plain.size(); ++i)
    out[i + 1] = static_cast<std::uint8_t>(plain[i] ^ out[0]);
  return out;
}

std::optional<OffsetData> fetch_offsets(const char* endpoint) {
  if (!endpoint) return std::nullopt;
  auto resp = https_get(endpoint, "RealOffsetFetcher/2.0", 15000);
  if (!resp || resp->status_code != 200)
    return std::nullopt;

  const auto& encrypted = resp->body;
  if (encrypted.size() < 13)
    return std::nullopt;

  // Layout: [key:1][xor'd plain...]
  const std::uint8_t key_byte = encrypted[0];
  std::vector<std::uint8_t> decrypted(encrypted.size() - 1);
  for (std::size_t i = 0; i < decrypted.size(); ++i)
    decrypted[i] = static_cast<std::uint8_t>(encrypted[i + 1] ^ key_byte);

  if (decrypted.size() < 8)
    return std::nullopt;

  const std::uint32_t cs2_version = read_u32_le(decrypted.data());
  const std::uint32_t stored_crc = read_u32_le(decrypted.data() + 4);
  const std::size_t data_offset = 8;
  const std::size_t data_len = decrypted.size() - data_offset;
  if (crc32(decrypted.data() + data_offset, data_len) != stored_crc)
    return std::nullopt;

  OffsetData offsets{};
  offsets.cs2Version = cs2_version;
  offsets.checksum = stored_crc;

  const std::uint8_t* p = decrypted.data() + data_offset;
  std::size_t remaining = data_len;
  auto read_u64 = [&](std::uint64_t& out) -> bool {
    if (remaining < 8) return false;
    out = read_u64_le(p);
    p += 8;
    remaining -= 8;
    return true;
  };

  if (!read_u64(offsets.dwEntityList)) return std::nullopt;
  if (!read_u64(offsets.dwLocalPlayer)) return std::nullopt;
  if (!read_u64(offsets.dwViewMatrix)) return std::nullopt;
  if (!read_u64(offsets.dwViewAngles)) return std::nullopt;
  if (!read_u64(offsets.dwGlowObjectManager)) return std::nullopt;
  if (!read_u64(offsets.dwSensitivity)) return std::nullopt;
  if (!read_u64(offsets.dwSensitivitySensitivity)) return std::nullopt;
  if (!read_u64(offsets.dwForceJump)) return std::nullopt;
  if (!read_u64(offsets.dwForceAttack)) return std::nullopt;
  if (!read_u64(offsets.dwPlayerResource)) return std::nullopt;

  // Hold a secure copy so sensitive offsets do not linger only in stack plaintext
  // longer than needed — material still returned by value for callers.
  SecureBuffer secure(reinterpret_cast<const std::uint8_t*>(&offsets), sizeof(offsets));
  (void)secure;
  return offsets;
}

bool verify_offsets(const OffsetData& offsets, std::uint32_t expected_cs2_version) {
  if (offsets.cs2Version == 0)
    return false;
  if (offsets.cs2Version != expected_cs2_version)
    return false;
  if (offsets.dwEntityList == 0 || offsets.dwLocalPlayer == 0 ||
      offsets.dwViewMatrix == 0)
    return false;
  return true;
}

Result<std::vector<std::uint8_t>> fetch_schema_offsets(
    const std::string& saas_url, const std::string& api_key) {
  HttpRequestOptions opts;
  opts.timeout_ms = 15000;
  opts.user_agent = "SchemaSaaS/2.0";
  if (!api_key.empty())
    opts.headers["X-Api-Key"] = api_key;
  opts.headers["Accept"] = "application/json, application/octet-stream";

  auto resp = https_get_ex(saas_url, opts);
  if (!resp) return Result<std::vector<std::uint8_t>>({}, resp.error_msg);
  if (resp->status_code < 200 || resp->status_code >= 300)
    return Result<std::vector<std::uint8_t>>(
        {}, "Schema SaaS status " + std::to_string(resp->status_code));
  return resp->body;
}

Result<std::vector<std::uint8_t>> fetch_encrypted_offsets(
    const std::string& c2_url, const std::vector<std::uint8_t>& key) {
  HttpRequestOptions opts;
  opts.timeout_ms = 15000;
  opts.user_agent = "OffsetC2/2.0";
  auto resp = https_get_ex(c2_url, opts);
  if (!resp) return Result<std::vector<std::uint8_t>>({}, resp.error_msg);
  if (resp->status_code < 200 || resp->status_code >= 300)
    return Result<std::vector<std::uint8_t>>(
        {}, "Encrypted offset status " + std::to_string(resp->status_code));

  std::vector<std::uint8_t> body = resp->body;
  if (key.empty()) return body;

  // Prefer ChaCha20 seal format (nonce||ct) when key is 32 bytes and body > 12
  if (key.size() == 32 && body.size() > 12) {
    auto opened = chacha20_open(key, body.data(), body.size());
    if (opened) return opened;
  }
  // XOR stream fallback
  for (std::size_t i = 0; i < body.size(); ++i)
    body[i] ^= key[i % key.size()];
  return body;
}

}  // namespace real::net
