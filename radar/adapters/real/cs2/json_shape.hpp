// json_shape.hpp: bounded shape scanner for the runtime offsets snapshot
// (E66 / S3).
//
// The loader used to accept `"key": value` anywhere in the text, so a key
// nested under an unrelated object could spoof an offset. This scanner
// records the depth and enclosing object name of every property and lets the
// loader require top-level or `fields`/`globals` placement.
//
// Not a general JSON parser: it tracks strings, escapes and container depth
// only, which is all the shape check needs.
//
// Pure and host-testable.

#pragma once

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace real::cs2::json {

struct KeyLocation {
  std::size_t pos = 0;  ///< first character of the key text
  int depth = 0;
  std::string parent;
};

inline std::vector<KeyLocation> key_locations(const std::string& text,
                                              const std::string& needle) {
  std::vector<KeyLocation> found;
  std::vector<std::string> stack;
  std::string pending_parent;
  std::string current_key;
  std::size_t key_start = 0;
  bool in_string = false;
  bool escape = false;
  int depth = 0;

  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (in_string) {
      if (escape) {
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == '"') {
        in_string = false;
        current_key.assign(text, key_start, i - key_start);
        std::size_t next = i + 1;
        while (next < text.size() &&
               std::isspace(static_cast<unsigned char>(text[next]))) {
          ++next;
        }
        if (next < text.size() && text[next] == ':' && current_key == needle) {
          found.push_back(
              {i - needle.size(), depth,
               stack.empty() ? std::string{}: stack.back()});
        }
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
      key_start = i + 1;
      continue;
    }
    if (c == '{' || c == '[') {
      stack.push_back(pending_parent);
      pending_parent.clear();
      ++depth;
      continue;
    }
    if (c == '}' || c == ']') {
      if (!stack.empty()) stack.pop_back();
      if (depth > 0) --depth;
      continue;
    }
    if (c == ':') {
      pending_parent = current_key;
      current_key.clear();
    }
  }
  return found;
}

inline bool is_object(const std::string& text) {
  for (const char c: text) {
    if (!std::isspace(static_cast<unsigned char>(c))) return c == '{';
  }
  return false;
}

/// Top-level, or directly under `fields`/`globals`.
inline bool key_shape_ok(const KeyLocation& location) {
  return location.depth == 1 ||
         (location.depth == 2 &&
          (location.parent == "fields" || location.parent == "globals"));
}

/// Read the unsigned integer value that follows `location`. The location points
/// at the first key character, so skip the rest of the key and the closing
/// quote before looking for the colon. Returns false on malformed input.
inline bool number_after(const std::string& text, const KeyLocation& location,
                         std::size_t key_length, std::uint64_t& out) {
  std::size_t pos = location.pos + key_length;
  if (pos < text.size() && text[pos] == '"') ++pos;
  while (pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[pos]))) {
    ++pos;
  }
  if (pos >= text.size() || text[pos] != ':') return false;
  ++pos;
  while (pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[pos]))) {
    ++pos;
  }
  if (pos >= text.size()) return false;
  if (text[pos] == '-' || text[pos] == '+' || text[pos] == '"') return false;
  const char* first = text.data() + pos;
  const char* last = text.data() + text.size();
  const auto [end, error] = std::from_chars(first, last, out, 10);
  if (error != std::errc{} || end == first) return false;
  if (end != last && *end != ',' && *end != '}' &&
      !std::isspace(static_cast<unsigned char>(*end))) {
    return false;
  }
  return true;
}

}  // namespace real::cs2::json
