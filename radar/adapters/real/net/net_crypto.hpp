// net_crypto.hpp — Self-contained cryptographic primitives for the net stack.
// No OpenSSL / third-party deps. SHA-256, HMAC-SHA256, ChaCha20, HKDF-like
// expand, CRC32, Base64/Base32/Hex, secure zeroing, and SecureBuffer.

#pragma once

#include "real/error.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace real::net {

// ── Secure memory ──────────────────────────────────────────────────

/// Best-effort wipe of sensitive bytes (survives compiler optimizers).
void secure_zero(void* p, std::size_t n);

/// XOR-encrypted in-memory buffer; decrypts on get(), zeros on destroy.
class SecureBuffer {
public:
  SecureBuffer() = default;
  SecureBuffer(const std::uint8_t* data, std::size_t len);
  explicit SecureBuffer(const std::vector<std::uint8_t>& data);
  ~SecureBuffer();

  SecureBuffer(SecureBuffer&& other) noexcept;
  SecureBuffer& operator=(SecureBuffer&& other) noexcept;
  SecureBuffer(const SecureBuffer&) = delete;
  SecureBuffer& operator=(const SecureBuffer&) = delete;

  /// Returns pointer to a temporary decrypted view (invalidated by next get/destroy).
  const std::uint8_t* get() const;
  std::vector<std::uint8_t> copy_plain() const;

  bool empty() const { return size_ == 0; }
  std::size_t size() const { return size_; }

private:
  void destroy();
  std::unique_ptr<std::uint8_t[]> data_;
  std::size_t size_ = 0;
  std::uint8_t key_ = 0xA5;
  mutable std::vector<std::uint8_t> decrypt_buf_;
};

// ── Hash / MAC ─────────────────────────────────────────────────────

/// SHA-256 digest (32 bytes).
std::vector<std::uint8_t> sha256(const std::uint8_t* data, std::size_t len);
inline std::vector<std::uint8_t> sha256(const std::vector<std::uint8_t>& v) {
  return sha256(v.data(), v.size());
}
std::vector<std::uint8_t> sha256(const std::string& s);

/// HMAC-SHA256 (32 bytes).
std::vector<std::uint8_t> hmac_sha256(const std::uint8_t* key, std::size_t key_len,
                                       const std::uint8_t* data, std::size_t data_len);
inline std::vector<std::uint8_t> hmac_sha256(const std::vector<std::uint8_t>& key,
                                              const std::vector<std::uint8_t>& data) {
  return hmac_sha256(key.data(), key.size(), data.data(), data.size());
}

/// CRC-32 (IEEE polynomial).
std::uint32_t crc32(const std::uint8_t* data, std::size_t len);
inline std::uint32_t crc32(const std::vector<std::uint8_t>& v) {
  return crc32(v.data(), v.size());
}

// ── Stream cipher ──────────────────────────────────────────────────

/// ChaCha20 encrypt/decrypt (in-place). counter starts at 0 unless specified.
void chacha20_xor(const std::uint8_t key[32], const std::uint8_t nonce[12],
                  std::uint8_t* data, std::size_t len, std::uint32_t counter = 0);

/// Encrypt payload with ChaCha20; prepends 12-byte random nonce.
/// Output: nonce(12) || ciphertext.
Result<std::vector<std::uint8_t>> chacha20_seal(
    const std::vector<std::uint8_t>& key32,
    const std::uint8_t* plain, std::size_t plain_len);

/// Decrypt nonce||ciphertext produced by chacha20_seal.
Result<std::vector<std::uint8_t>> chacha20_open(
    const std::vector<std::uint8_t>& key32,
    const std::uint8_t* sealed, std::size_t sealed_len);

// ── Key derivation ─────────────────────────────────────────────────

/// HKDF-Extract-like: HMAC-SHA256(salt, ikm). If salt empty, zeros used.
std::vector<std::uint8_t> hkdf_extract(const std::vector<std::uint8_t>& salt,
                                        const std::vector<std::uint8_t>& ikm);

/// HKDF-Expand-like: produce `out_len` bytes from PRK + info.
std::vector<std::uint8_t> hkdf_expand(const std::vector<std::uint8_t>& prk,
                                       const std::string& info, std::size_t out_len);

/// Derive a 32-byte session key from password/token + optional salt string.
std::vector<std::uint8_t> derive_session_key(const std::string& token,
                                              const std::string& salt = "ac-lab-c2");

// ── Encoding ───────────────────────────────────────────────────────

std::string to_hex(const std::uint8_t* data, std::size_t len, bool upper = false);
inline std::string to_hex(const std::vector<std::uint8_t>& v, bool upper = false) {
  return to_hex(v.data(), v.size(), upper);
}
Result<std::vector<std::uint8_t>> from_hex(const std::string& hex);

std::string to_base64(const std::uint8_t* data, std::size_t len);
inline std::string to_base64(const std::vector<std::uint8_t>& v) {
  return to_base64(v.data(), v.size());
}
Result<std::vector<std::uint8_t>> from_base64(const std::string& b64);

/// RFC 4648 base32 (A-Z2-7) without padding — DNS-label safe.
std::string to_base32(const std::uint8_t* data, std::size_t len);
inline std::string to_base32(const std::vector<std::uint8_t>& v) {
  return to_base32(v.data(), v.size());
}
Result<std::vector<std::uint8_t>> from_base32(const std::string& b32);

// ── Random / timing ────────────────────────────────────────────────

/// Cryptographically strong when OS RNG available; else mix of counters.
Result<std::vector<std::uint8_t>> random_bytes(std::size_t n);

/// Uniform integer in [lo, hi] inclusive.
std::uint32_t random_u32(std::uint32_t lo, std::uint32_t hi);

/// Jitter around base_ms: base ± percent (e.g. 20 => ±20%).
int jitter_ms(int base_ms, int percent = 20);

// ── Endian helpers ─────────────────────────────────────────────────

inline void write_u16_le(std::uint8_t* p, std::uint16_t v) {
  p[0] = static_cast<std::uint8_t>(v);
  p[1] = static_cast<std::uint8_t>(v >> 8);
}
inline void write_u32_le(std::uint8_t* p, std::uint32_t v) {
  p[0] = static_cast<std::uint8_t>(v);
  p[1] = static_cast<std::uint8_t>(v >> 8);
  p[2] = static_cast<std::uint8_t>(v >> 16);
  p[3] = static_cast<std::uint8_t>(v >> 24);
}
inline void write_u64_le(std::uint8_t* p, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
inline std::uint16_t read_u16_le(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0]) |
         (static_cast<std::uint16_t>(p[1]) << 8);
}
inline std::uint32_t read_u32_le(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) |
         (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}
inline std::uint64_t read_u64_le(const std::uint8_t* p) {
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i)
    v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
  return v;
}
inline void write_u16_be(std::uint8_t* p, std::uint16_t v) {
  p[0] = static_cast<std::uint8_t>(v >> 8);
  p[1] = static_cast<std::uint8_t>(v);
}
inline void write_u32_be(std::uint8_t* p, std::uint32_t v) {
  p[0] = static_cast<std::uint8_t>(v >> 24);
  p[1] = static_cast<std::uint8_t>(v >> 16);
  p[2] = static_cast<std::uint8_t>(v >> 8);
  p[3] = static_cast<std::uint8_t>(v);
}
inline std::uint16_t read_u16_be(const std::uint8_t* p) {
  return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}
inline std::uint32_t read_u32_be(const std::uint8_t* p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) |
         (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) |
         static_cast<std::uint32_t>(p[3]);
}

}  // namespace real::net
