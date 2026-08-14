// crypto.cpp — SHA-256, HMAC, ChaCha20, HKDF, encodings, secure buffer.

#include "real/net/net_crypto.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include <bcrypt.h>
#  pragma comment(lib, "bcrypt.lib")
#endif

namespace real::net {

// ── Secure zero ────────────────────────────────────────────────────

void secure_zero(void* p, std::size_t n) {
  if (!p || n == 0) return;
#if LR_PLATFORM_WINDOWS
  SecureZeroMemory(p, n);
#else
  volatile std::uint8_t* v = static_cast<volatile std::uint8_t*>(p);
  while (n--) *v++ = 0;
#endif
}

// ── SecureBuffer ───────────────────────────────────────────────────

SecureBuffer::SecureBuffer(const std::uint8_t* data, std::size_t len)
    : size_(len), key_(0xA5) {
  if (len == 0 || !data) return;
  // Per-buffer key from first mix of content + length (still XOR-only obfuscation).
  key_ = static_cast<std::uint8_t>(0xA5 ^ (len & 0xFF) ^ (data[0]));
  if (key_ == 0) key_ = 0x5A;
  data_ = std::make_unique<std::uint8_t[]>(len);
  for (std::size_t i = 0; i < len; ++i)
    data_[i] = static_cast<std::uint8_t>(data[i] ^ key_ ^ static_cast<std::uint8_t>(i));
}

SecureBuffer::SecureBuffer(const std::vector<std::uint8_t>& data)
    : SecureBuffer(data.data(), data.size()) {}

SecureBuffer::~SecureBuffer() { destroy(); }

SecureBuffer::SecureBuffer(SecureBuffer&& other) noexcept
    : data_(std::move(other.data_)), size_(other.size_), key_(other.key_) {
  other.size_ = 0;
  other.key_ = 0;
}

SecureBuffer& SecureBuffer::operator=(SecureBuffer&& other) noexcept {
  if (this != &other) {
    destroy();
    data_ = std::move(other.data_);
    size_ = other.size_;
    key_ = other.key_;
    other.size_ = 0;
    other.key_ = 0;
  }
  return *this;
}

const std::uint8_t* SecureBuffer::get() const {
  if (!data_ || size_ == 0) return nullptr;
  decrypt_buf_.resize(size_);
  for (std::size_t i = 0; i < size_; ++i)
    decrypt_buf_[i] = static_cast<std::uint8_t>(data_[i] ^ key_ ^ static_cast<std::uint8_t>(i));
  return decrypt_buf_.data();
}

std::vector<std::uint8_t> SecureBuffer::copy_plain() const {
  const std::uint8_t* p = get();
  if (!p) return {};
  return std::vector<std::uint8_t>(p, p + size_);
}

void SecureBuffer::destroy() {
  if (data_) {
    secure_zero(data_.get(), size_);
    data_.reset();
    size_ = 0;
  }
  if (!decrypt_buf_.empty()) {
    secure_zero(decrypt_buf_.data(), decrypt_buf_.size());
    decrypt_buf_.clear();
  }
}

// ── SHA-256 (FIPS 180-4) ───────────────────────────────────────────

namespace {

constexpr std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

void sha256_transform(std::uint32_t state[8], const std::uint8_t block[64]) {
  static const std::uint32_t K[64] = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
      0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
      0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
      0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
      0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
      0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
      0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
      0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
      0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i)
    w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
           (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
           static_cast<std::uint32_t>(block[i * 4 + 3]);
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
    h = g; g = f; f = e; e = d + t1;
    d = c; c = b; b = a; a = t1 + t2;
  }
  state[0] += a; state[1] += b; state[2] += c; state[3] += d;
  state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

}  // namespace

std::vector<std::uint8_t> sha256(const std::uint8_t* data, std::size_t len) {
  std::uint32_t state[8] = {
      0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

  std::uint8_t block[64];
  std::size_t offset = 0;
  while (len - offset >= 64) {
    std::memcpy(block, data + offset, 64);
    sha256_transform(state, block);
    offset += 64;
  }

  const std::size_t rem = len - offset;
  std::memcpy(block, data + offset, rem);
  block[rem] = 0x80;
  if (rem < 56) {
    std::memset(block + rem + 1, 0, 55 - rem);
  } else {
    std::memset(block + rem + 1, 0, 63 - rem);
    sha256_transform(state, block);
    std::memset(block, 0, 56);
  }
  const std::uint64_t bit_len = static_cast<std::uint64_t>(len) * 8;
  for (int i = 0; i < 8; ++i)
    block[63 - i] = static_cast<std::uint8_t>(bit_len >> (8 * i));
  sha256_transform(state, block);

  std::vector<std::uint8_t> out(32);
  for (int i = 0; i < 8; ++i) {
    out[i * 4]     = static_cast<std::uint8_t>(state[i] >> 24);
    out[i * 4 + 1] = static_cast<std::uint8_t>(state[i] >> 16);
    out[i * 4 + 2] = static_cast<std::uint8_t>(state[i] >> 8);
    out[i * 4 + 3] = static_cast<std::uint8_t>(state[i]);
  }
  return out;
}

std::vector<std::uint8_t> sha256(const std::string& s) {
  return sha256(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

std::vector<std::uint8_t> hmac_sha256(const std::uint8_t* key, std::size_t key_len,
                                      const std::uint8_t* data, std::size_t data_len) {
  std::uint8_t k[64]{};
  if (key_len > 64) {
    auto hk = sha256(key, key_len);
    std::memcpy(k, hk.data(), 32);
  } else {
    std::memcpy(k, key, key_len);
  }

  std::uint8_t ipad[64], opad[64];
  for (int i = 0; i < 64; ++i) {
    ipad[i] = static_cast<std::uint8_t>(k[i] ^ 0x36);
    opad[i] = static_cast<std::uint8_t>(k[i] ^ 0x5c);
  }

  std::vector<std::uint8_t> inner;
  inner.reserve(64 + data_len);
  inner.insert(inner.end(), ipad, ipad + 64);
  if (data && data_len)
    inner.insert(inner.end(), data, data + data_len);
  auto inner_hash = sha256(inner.data(), inner.size());

  std::vector<std::uint8_t> outer;
  outer.reserve(64 + 32);
  outer.insert(outer.end(), opad, opad + 64);
  outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());
  return sha256(outer.data(), outer.size());
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t len) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j)
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
  }
  return crc ^ 0xFFFFFFFFu;
}

// ── ChaCha20 (RFC 8439) ────────────────────────────────────────────

namespace {

void quarter_round(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c, std::uint32_t& d) {
  a += b; d ^= a; d = (d << 16) | (d >> 16);
  c += d; b ^= c; b = (b << 12) | (b >> 20);
  a += b; d ^= a; d = (d << 8)  | (d >> 24);
  c += d; b ^= c; b = (b << 7)  | (b >> 25);
}

void chacha20_block(const std::uint32_t in[16], std::uint8_t out[64]) {
  std::uint32_t x[16];
  std::memcpy(x, in, 64);
  for (int i = 0; i < 10; ++i) {
    quarter_round(x[0], x[4], x[8],  x[12]);
    quarter_round(x[1], x[5], x[9],  x[13]);
    quarter_round(x[2], x[6], x[10], x[14]);
    quarter_round(x[3], x[7], x[11], x[15]);
    quarter_round(x[0], x[5], x[10], x[15]);
    quarter_round(x[1], x[6], x[11], x[12]);
    quarter_round(x[2], x[7], x[8],  x[13]);
    quarter_round(x[3], x[4], x[9],  x[14]);
  }
  for (int i = 0; i < 16; ++i) x[i] += in[i];
  for (int i = 0; i < 16; ++i) {
    out[i * 4]     = static_cast<std::uint8_t>(x[i]);
    out[i * 4 + 1] = static_cast<std::uint8_t>(x[i] >> 8);
    out[i * 4 + 2] = static_cast<std::uint8_t>(x[i] >> 16);
    out[i * 4 + 3] = static_cast<std::uint8_t>(x[i] >> 24);
  }
}

}  // namespace

void chacha20_xor(const std::uint8_t key[32], const std::uint8_t nonce[12],
                  std::uint8_t* data, std::size_t len, std::uint32_t counter) {
  if (!data || len == 0) return;
  std::uint32_t state[16];
  state[0] = 0x61707865u;
  state[1] = 0x3320646eu;
  state[2] = 0x79622d32u;
  state[3] = 0x6b206574u;
  for (int i = 0; i < 8; ++i)
    state[4 + i] = read_u32_le(key + i * 4);
  state[12] = counter;
  state[13] = read_u32_le(nonce);
  state[14] = read_u32_le(nonce + 4);
  state[15] = read_u32_le(nonce + 8);

  std::uint8_t block[64];
  std::size_t off = 0;
  while (off < len) {
    chacha20_block(state, block);
    ++state[12];
    const std::size_t n = (std::min)(static_cast<std::size_t>(64), len - off);
    for (std::size_t i = 0; i < n; ++i)
      data[off + i] ^= block[i];
    off += n;
  }
  secure_zero(block, sizeof(block));
  secure_zero(state, sizeof(state));
}

Result<std::vector<std::uint8_t>> chacha20_seal(
    const std::vector<std::uint8_t>& key32,
    const std::uint8_t* plain, std::size_t plain_len) {
  if (key32.size() != 32)
    return Result<std::vector<std::uint8_t>>({}, "ChaCha20 key must be 32 bytes");
  auto nonce_r = random_bytes(12);
  if (!nonce_r) return Result<std::vector<std::uint8_t>>({}, nonce_r.error_msg);
  std::vector<std::uint8_t> out;
  out.reserve(12 + plain_len);
  out.insert(out.end(), nonce_r->begin(), nonce_r->end());
  if (plain && plain_len)
    out.insert(out.end(), plain, plain + plain_len);
  if (plain_len)
    chacha20_xor(key32.data(), out.data(), out.data() + 12, plain_len, 0);
  return out;
}

Result<std::vector<std::uint8_t>> chacha20_open(
    const std::vector<std::uint8_t>& key32,
    const std::uint8_t* sealed, std::size_t sealed_len) {
  if (key32.size() != 32)
    return Result<std::vector<std::uint8_t>>({}, "ChaCha20 key must be 32 bytes");
  if (!sealed || sealed_len < 12)
    return Result<std::vector<std::uint8_t>>({}, "Sealed blob too short");
  std::vector<std::uint8_t> plain(sealed + 12, sealed + sealed_len);
  if (!plain.empty())
    chacha20_xor(key32.data(), sealed, plain.data(), plain.size(), 0);
  return plain;
}

// ── HKDF ───────────────────────────────────────────────────────────

std::vector<std::uint8_t> hkdf_extract(const std::vector<std::uint8_t>& salt,
                                        const std::vector<std::uint8_t>& ikm) {
  std::vector<std::uint8_t> s = salt;
  if (s.empty()) s.assign(32, 0);
  return hmac_sha256(s.data(), s.size(), ikm.data(), ikm.size());
}

std::vector<std::uint8_t> hkdf_expand(const std::vector<std::uint8_t>& prk,
                                       const std::string& info, std::size_t out_len) {
  std::vector<std::uint8_t> out;
  out.reserve(out_len);
  std::vector<std::uint8_t> t;
  std::uint8_t counter = 1;
  while (out.size() < out_len) {
    std::vector<std::uint8_t> msg;
    msg.insert(msg.end(), t.begin(), t.end());
    msg.insert(msg.end(), info.begin(), info.end());
    msg.push_back(counter++);
    t = hmac_sha256(prk.data(), prk.size(), msg.data(), msg.size());
    const std::size_t take = (std::min)(t.size(), out_len - out.size());
    out.insert(out.end(), t.begin(), t.begin() + static_cast<std::ptrdiff_t>(take));
  }
  return out;
}

std::vector<std::uint8_t> derive_session_key(const std::string& token,
                                             const std::string& salt) {
  std::vector<std::uint8_t> ikm(token.begin(), token.end());
  std::vector<std::uint8_t> salt_bytes(salt.begin(), salt.end());
  auto prk = hkdf_extract(salt_bytes, ikm);
  return hkdf_expand(prk, "session-key-v1", 32);
}

// ── Encodings ──────────────────────────────────────────────────────

std::string to_hex(const std::uint8_t* data, std::size_t len, bool upper) {
  static const char* lo = "0123456789abcdef";
  static const char* up = "0123456789ABCDEF";
  const char* hex = upper ? up : lo;
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2]     = hex[(data[i] >> 4) & 0xF];
    out[i * 2 + 1] = hex[data[i] & 0xF];
  }
  return out;
}

Result<std::vector<std::uint8_t>> from_hex(const std::string& hex) {
  if (hex.size() % 2 != 0)
    return Result<std::vector<std::uint8_t>>({}, "Odd hex length");
  std::vector<std::uint8_t> out;
  out.reserve(hex.size() / 2);
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const int hi = nibble(hex[i]);
    const int lo = nibble(hex[i + 1]);
    if (hi < 0 || lo < 0)
      return Result<std::vector<std::uint8_t>>({}, "Invalid hex char");
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return out;
}

std::string to_base64(const std::uint8_t* data, std::size_t len) {
  static const char* tbl =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  for (std::size_t i = 0; i < len; i += 3) {
    const std::uint32_t n = (static_cast<std::uint32_t>(data[i]) << 16) |
        ((i + 1 < len) ? (static_cast<std::uint32_t>(data[i + 1]) << 8) : 0) |
        ((i + 2 < len) ? static_cast<std::uint32_t>(data[i + 2]) : 0);
    out.push_back(tbl[(n >> 18) & 63]);
    out.push_back(tbl[(n >> 12) & 63]);
    out.push_back((i + 1 < len) ? tbl[(n >> 6) & 63] : '=');
    out.push_back((i + 2 < len) ? tbl[n & 63] : '=');
  }
  return out;
}

Result<std::vector<std::uint8_t>> from_base64(const std::string& b64) {
  auto val = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=' || c == '\n' || c == '\r' || c == ' ') return -2;
    return -1;
  };
  std::vector<std::uint8_t> out;
  int acc = 0, bits = 0;
  for (char c : b64) {
    const int v = val(c);
    if (v == -2) continue;
    if (v < 0) return Result<std::vector<std::uint8_t>>({}, "Invalid base64");
    acc = (acc << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFF));
    }
  }
  return out;
}

std::string to_base32(const std::uint8_t* data, std::size_t len) {
  static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  std::string out;
  std::uint32_t buffer = 0;
  int bits = 0;
  for (std::size_t i = 0; i < len; ++i) {
    buffer = (buffer << 8) | data[i];
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      out.push_back(tbl[(buffer >> bits) & 31]);
    }
  }
  if (bits > 0)
    out.push_back(tbl[(buffer << (5 - bits)) & 31]);
  return out;
}

Result<std::vector<std::uint8_t>> from_base32(const std::string& b32) {
  auto val = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    if (c == '=' || c == ' ') return -2;
    return -1;
  };
  std::vector<std::uint8_t> out;
  std::uint32_t buffer = 0;
  int bits = 0;
  for (char c : b32) {
    const int v = val(c);
    if (v == -2) continue;
    if (v < 0) return Result<std::vector<std::uint8_t>>({}, "Invalid base32");
    buffer = (buffer << 5) | static_cast<std::uint32_t>(v);
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
    }
  }
  return out;
}

// ── Random ─────────────────────────────────────────────────────────

Result<std::vector<std::uint8_t>> random_bytes(std::size_t n) {
  std::vector<std::uint8_t> out(n);
  if (n == 0) return out;
#if LR_PLATFORM_WINDOWS
  NTSTATUS st = BCryptGenRandom(nullptr, out.data(), static_cast<ULONG>(n),
                                BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (st < 0) {
    // Fallback: std::random_device mix
    std::random_device rd;
    for (std::size_t i = 0; i < n; ++i)
      out[i] = static_cast<std::uint8_t>(rd());
  }
#else
  FILE* f = std::fopen("/dev/urandom", "rb");
  if (f) {
    const std::size_t got = std::fread(out.data(), 1, n, f);
    std::fclose(f);
    if (got != n) {
      std::random_device rd;
      for (std::size_t i = got; i < n; ++i)
        out[i] = static_cast<std::uint8_t>(rd());
    }
  } else {
    std::random_device rd;
    for (std::size_t i = 0; i < n; ++i)
      out[i] = static_cast<std::uint8_t>(rd());
  }
#endif
  return out;
}

std::uint32_t random_u32(std::uint32_t lo, std::uint32_t hi) {
  if (hi < lo) std::swap(lo, hi);
  auto r = random_bytes(4);
  std::uint32_t v = 0;
  if (r && r->size() == 4)
    v = read_u32_le(r->data());
  else {
    const auto t = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    v = static_cast<std::uint32_t>(t);
  }
  const std::uint64_t span = static_cast<std::uint64_t>(hi - lo) + 1ull;
  return lo + static_cast<std::uint32_t>(v % span);
}

int jitter_ms(int base_ms, int percent) {
  if (base_ms <= 0) return 0;
  if (percent <= 0) return base_ms;
  const int delta = (base_ms * percent) / 100;
  if (delta <= 0) return base_ms;
  const int off = static_cast<int>(random_u32(0, static_cast<std::uint32_t>(delta * 2))) - delta;
  const int v = base_ms + off;
  return v < 1 ? 1 : v;
}

}  // namespace real::net
