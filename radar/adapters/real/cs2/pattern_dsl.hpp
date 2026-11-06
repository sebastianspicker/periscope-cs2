// pattern_dsl.hpp — Explicit signature DSL for AOB scanning (E56 / S1).
//
// The old `Pattern::from_bytes` treats every `0x00` byte as a wildcard, so a
// literal zero byte can never be matched, and `relative_displacement_offset`
// grabs the first `????` anywhere in the mask. This parser marks wildcards
// explicitly and records the resolution operator by position.
//
// Grammar (mirrors the donor's research syntax, reimplemented):
//   `48 8B 0D ?? ?? ?? ??`  bytes, `??` or `?` = one wildcard byte
//   `>`  next 4 bytes are a rel32 call operand; resolve from operand+4
//   `*`  next 4 bytes are a RIP-relative operand; resolve from operand+4
//   `^`  next 8 bytes are an absolute pointer; read it
//   `~`  dereference the final resolved pointer once
//   `+1A` / `-20` add a signed post-offset (hex)
//
// A pattern has at most one resolution operator; a second one is an error.
// `deref_final` without an operator is allowed (direct absolute read).
//
// Pure and host-testable: no platform headers, no I/O.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace real::cs2::sig {

enum class ResolveOp : std::uint8_t {
  Direct,
  RelCall,
  RipRelative,
  AbsolutePtr,
};

struct ParsedPattern {
  std::vector<std::uint8_t> bytes;
  std::vector<bool> wildcard;
  ResolveOp op = ResolveOp::Direct;
  std::size_t op_offset = 0;
  std::int64_t post_offset = 0;
  bool deref_final = false;
  std::string error;

  bool ok() const { return error.empty() && !bytes.empty(); }
  bool has_wildcards() const {
    for (const bool w : wildcard) {
      if (w) return true;
    }
    return false;
  }
};

namespace detail {

inline int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

inline bool set_operator(ParsedPattern& out, ResolveOp op, std::size_t at) {
  if (out.op != ResolveOp::Direct) {
    out.error = "multiple resolution operators";
    return false;
  }
  out.op = op;
  out.op_offset = at;
  return true;
}

}  // namespace detail

/// Parse a textual pattern. Returns false and fills `out.error` on failure.
inline bool parse_pattern(std::string_view text, ParsedPattern& out,
                          std::size_t max_bytes = 4096) {
  out = ParsedPattern{};
  std::size_t i = 0;
  while (i < text.size()) {
    const char c = text[i];

    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      ++i;
      continue;
    }

    if (c == '>') {
      if (!detail::set_operator(out, ResolveOp::RelCall, out.bytes.size())) return false;
      ++i;
      continue;
    }
    if (c == '*') {
      if (!detail::set_operator(out, ResolveOp::RipRelative, out.bytes.size())) return false;
      ++i;
      continue;
    }
    if (c == '^') {
      if (!detail::set_operator(out, ResolveOp::AbsolutePtr, out.bytes.size())) return false;
      ++i;
      continue;
    }
    if (c == '~') {
      out.deref_final = true;
      ++i;
      continue;
    }
    if (c == '+' || c == '-') {
      const bool negate = (c == '-');
      ++i;
      std::int64_t value = 0;
      std::size_t digits = 0;
      while (i < text.size()) {
        const int h = detail::hex_digit(text[i]);
        if (h < 0) break;
        value = (value << 4) | h;
        ++i;
        ++digits;
      }
      if (digits == 0) {
        out.error = "post-offset without digits";
        return false;
      }
      out.post_offset = negate ? -value : value;
      continue;
    }
    if (c == '?') {
      if (out.bytes.size() >= max_bytes) {
        out.error = "pattern too long";
        return false;
      }
      out.bytes.push_back(0);
      out.wildcard.push_back(true);
      ++i;
      if (i < text.size() && text[i] == '?') ++i;  // accept `??`
      continue;
    }

    const int high = detail::hex_digit(c);
    if (high < 0) {
      out.error = std::string("unexpected character: ") + c;
      return false;
    }
    ++i;
    int value = high;
    if (i < text.size()) {
      const int low = detail::hex_digit(text[i]);
      if (low >= 0) {
        value = (high << 4) | low;
        ++i;
      }
    }
    if (out.bytes.size() >= max_bytes) {
      out.error = "pattern too long";
      return false;
    }
    out.bytes.push_back(static_cast<std::uint8_t>(value));
    out.wildcard.push_back(false);
  }

  if (out.bytes.empty()) {
    out.error = "empty pattern";
    return false;
  }
  return true;
}

/// Match the pattern bytes at `at` inside `buffer`.
inline bool matches_at(const std::uint8_t* buffer, std::size_t size,
                       std::size_t at, const ParsedPattern& pattern) {
  if (at > size || pattern.bytes.size() > size - at) return false;
  for (std::size_t i = 0; i < pattern.bytes.size(); ++i) {
    if (!pattern.wildcard[i] && buffer[at + i] != pattern.bytes[i]) {
      return false;
    }
  }
  return true;
}

/// Resolve the operator for a match. `chunk` holds the scanned bytes,
/// `match_index` is the match offset inside it, `match_address` is the
/// absolute address of `chunk[0] + match_index`. Returns 0 on a short operand.
inline std::uint64_t resolve_match(const std::uint8_t* chunk, std::size_t size,
                                   std::size_t match_index,
                                   std::uint64_t match_address,
                                   const ParsedPattern& pattern) {
  std::uint64_t result = match_address;
  std::size_t operand = 0;
  switch (pattern.op) {
    case ResolveOp::Direct:
      break;
    case ResolveOp::RelCall:
      operand = match_index + pattern.op_offset + 1;  // skip the call opcode
      break;
    case ResolveOp::RipRelative:
      operand = match_index + pattern.op_offset;
      break;
    case ResolveOp::AbsolutePtr:
      operand = match_index + pattern.op_offset;
      break;
  }

  if (pattern.op != ResolveOp::Direct) {
    if (pattern.op == ResolveOp::AbsolutePtr) {
      if (operand > size || sizeof(std::uint64_t) > size - operand) return 0;
      std::uint64_t pointer = 0;
      for (std::size_t i = 0; i < sizeof(pointer); ++i) {
        pointer |= static_cast<std::uint64_t>(chunk[operand + i]) << (8 * i);
      }
      result = pointer;
    } else {
      if (operand > size || sizeof(std::int32_t) > size - operand) return 0;
      std::uint32_t raw = 0;
      for (std::size_t i = 0; i < sizeof(raw); ++i) {
        raw |= static_cast<std::uint32_t>(chunk[operand + i]) << (8 * i);
      }
      const std::int32_t relative = static_cast<std::int32_t>(raw);
      result = static_cast<std::uint64_t>(
          static_cast<std::int64_t>(match_address + pattern.op_offset +
                                    (pattern.op == ResolveOp::RelCall ? 5 : 4)) +
          relative);
    }
  }

  result = static_cast<std::uint64_t>(
      static_cast<std::int64_t>(result) + pattern.post_offset);

  if (pattern.deref_final) {
    // The caller must supply a reader for a final dereference; this helper
    // cannot know the target address space. A 0 result means "needs read".
    if (result == 0) return 0;
  }
  return result;
}

/// True when the pattern needs a final dereference (the caller reads the
/// pointer at the resolved address).
inline bool needs_final_read(const ParsedPattern& pattern) {
  return pattern.deref_final;
}

}  // namespace real::cs2::sig
