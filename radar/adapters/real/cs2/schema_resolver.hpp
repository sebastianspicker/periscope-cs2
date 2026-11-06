// schema_resolver.hpp — data-only runtime schema field lookup (E55).
//
// The E13 walker resolves a field once a class-info pointer is known. The
// missing seam was *discovery*. This resolver is the external-process
// compromise: it does not call the game's `FindDeclaredClass` vfunc (which
// would need remote code execution); it scans the supplied type scope for a
// class-info candidate and validates the class name and field hash by
// reading, exactly the data the vfunc would return.
//
// The scope scan is bounded (`kMaxScopeScan`) and runs once per field, then
// the result is memoized by the caller. A wrong candidate fails validation
// and the scan continues; a match requires the class name and the FNV field
// hash to agree.
//
// Pure over a read functor and host-testable.

#pragma once

#include "real/cs2/schema_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace real::cs2::schema {

struct FieldLookup {
  bool found = false;
  std::uint64_t offset = 0;
  std::uint64_t class_info = 0;
};

namespace detail {

template <typename ReadFn>
inline bool read_cstr_contains(std::uint64_t address, const char* needle,
                               std::size_t max_len, ReadFn& read) {
  if (address == 0 || needle == nullptr || *needle == '\0') return false;
  (void)max_len;
  char buffer[160]{};
  if (!read(address, buffer, sizeof(buffer) - 1)) return false;
  return std::strstr(buffer, needle) != nullptr;
}

}  // namespace detail

/// Scan `scope` for a class-info whose class name contains `class_name` and
/// whose field table contains `field_name` (FNV-1a match). Bounded and
/// deliberately conservative.
template <typename ReadFn>
inline FieldLookup resolve_field_bruteforce(ReadFn read, std::uint64_t scope,
                                            const char* class_name,
                                            const char* field_name,
                                            const SchemaLayout& layout = {}) {
  FieldLookup result;
  if (scope == 0 || class_name == nullptr || field_name == nullptr) {
    return result;
  }
  const std::uint32_t field_hash = fnv1a(field_name);

  for (std::uint64_t off = 0; off < layout.max_scope_scan; off += 8) {
    std::uint64_t class_info = 0;
    if (!read(scope + off, &class_info, sizeof(class_info))) continue;
    if (class_info < 0x10000 || class_info == 0xFFFFFFFFFFFFFFFFull) continue;

    // Class name: pointer at class_info + class_name_offset.
    std::uint64_t name_ptr = 0;
    if (!read(class_info + layout.class_name_offset, &name_ptr,
              sizeof(name_ptr))) {
      continue;
    }
    if (name_ptr < 0x10000 || name_ptr == 0xFFFFFFFFFFFFFFFFull) continue;
    if (!detail::read_cstr_contains(name_ptr, class_name,
                                    layout.max_name_length, read)) {
      continue;
    }

    // Field table.
    std::uint64_t fields = 0;
    std::uint32_t count = 0;
    if (!read(class_info + layout.class_field_table_offset, &fields,
              sizeof(fields)) ||
        !read(class_info + layout.field_count_offset, &count, sizeof(count))) {
      continue;
    }
    if (fields < 0x10000 || count == 0 ||
        count > layout.max_fields_per_class) {
      continue;
    }

    for (std::uint32_t i = 0; i < count; ++i) {
      const std::uint64_t field = fields + static_cast<std::uint64_t>(i) *
                                               layout.field_stride;
      std::uint64_t field_name_ptr = 0;
      if (!read(field + layout.field_name_offset, &field_name_ptr,
                sizeof(field_name_ptr))) {
        break;
      }
      if (field_name_ptr < 0x10000 ||
          field_name_ptr == 0xFFFFFFFFFFFFFFFFull) {
        continue;
      }
      char name[96]{};
      if (!read(field_name_ptr, name, sizeof(name) - 1)) continue;
      if (fnv1a(name) != field_hash) continue;
      std::uint32_t offset = 0;
      if (!read(field + layout.field_offset_offset, &offset, sizeof(offset))) {
        continue;
      }
      result.found = true;
      result.offset = offset;
      result.class_info = class_info;
      return result;
    }
  }
  return result;
}

}  // namespace real::cs2::schema
