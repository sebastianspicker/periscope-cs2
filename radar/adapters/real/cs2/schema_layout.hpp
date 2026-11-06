// schema_layout.hpp — Pure Source-2 runtime-schema resolution planning
// (assessment ledger E13).
//
// The runtime snapshot (offsets_snapshot.hpp) is tier 1: fast, deterministic,
// but frozen at dump time. When a live game shifts a schema field, the
// snapshot silently misreads. Tier 2 is the game's own schema system: the
// process knows where its fields are, so a walker that follows
// SchemaSystem → CSchemaSystemTypeScope → declared classes → field tables
// recovers field offsets *from the game* instead of from a dump.
//
// This header holds the platform-neutral half:
//
//   - FNV-1a 32-bit name hashing (the identity the schema system uses for
//     field lookup; mirrors the lab's educational schema.hpp model).
//   - The field-table walk plan as data (fixed struct layouts with named
//     strides), so the Windows walker in schema_resolver.cpp contains no
//     magic numbers of its own.
//   - A memo (class+field → offset) with bounded size, so each field is
//     resolved from the game at most once per session.
//
// The Windows half (actually issuing reads through the tier backend) lives
// in schema_resolver.cpp; the pure half is host-testable against synthetic
// table images.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace real::cs2::schema {

/// FNV-1a 32-bit (the hash the schema lookup uses for field names).
inline constexpr std::uint32_t kFnvOffsetBasis = 0x811C9DC5u;
inline constexpr std::uint32_t kFnvPrime = 0x01000193u;

constexpr std::uint32_t fnv1a(const char* text, std::uint32_t hash = kFnvOffsetBasis) {
  while (*text != '\0') {
    hash ^= static_cast<std::uint8_t>(*text++);
    hash *= kFnvPrime;
  }
  return hash;
}

/// Fixed layout constants of the schema-system structures the walker reads.
/// Values are the ones documented for the current Source-2 schema system;
/// they are *data*, versioned here, never scattered through the walker.
struct SchemaLayout {
  std::uint64_t schema_system_vfunc_index = 13;  ///< SchemaSystem_001 vtable slot
  std::uint64_t find_declared_class_vfunc = 2;   ///< type-scope vtable slot
  std::uint64_t class_field_table_offset = 0x30;  ///< CSchemaClassInfo → fields
  std::size_t field_stride = 0x20;               ///< one SchemaFieldInfo entry
  std::uint64_t field_name_offset = 0x08;        ///< char* name pointer
  std::uint64_t field_offset_offset = 0x10;      ///< single-field offset
  std::uint64_t field_count_offset = 0x28;       ///< class field count (u32)
  std::size_t max_fields_per_class = 4096;       ///< walk bound (corrupt table)
  std::size_t max_name_length = 256;             ///< name read bound
};

/// One resolved field.
struct ResolvedField {
  std::string class_name;
  std::string field_name;
  std::uint64_t offset = 0;
};

/// Memo: class+field → offset. Bounded; oldest-insertion eviction is not
/// needed because the working set (schema classes the radar touches) is
/// small and stable — the bound exists to catch runaway misresolution.
class FieldMemo {
 public:
  explicit FieldMemo(std::size_t capacity = 512) : capacity_(capacity < 8 ? 8 : capacity) {}

  std::optional<std::uint64_t> lookup(const std::string& class_name,
                                      const std::string& field_name) const {
    const auto it = entries_.find(key(class_name, field_name));
    if (it == entries_.end()) return std::nullopt;
    return it->second;
  }

  void store(const std::string& class_name,
             const std::string& field_name,
             std::uint64_t offset) {
    if (entries_.size() >= capacity_) {
      entries_.clear();  // bounded reset: re-resolve on demand
    }
    entries_.emplace(key(class_name, field_name), offset);
  }

  std::size_t size() const { return entries_.size(); }

 private:
  static std::string key(const std::string& class_name, const std::string& field_name) {
    return class_name + "::" + field_name;
  }

  std::size_t capacity_;
  std::unordered_map<std::string, std::uint64_t> entries_;
};

/// A planned field-table probe: everything the walker needs to resolve one
/// field, derived from pure data (no I/O decisions here).
struct FieldProbe {
  std::uint64_t class_info_address = 0;
  std::uint64_t field_table_address = 0;
  std::uint32_t field_count = 0;
  std::uint32_t name_hash = 0;  ///< FNV-1a of the wanted field name
  std::size_t max_fields = 0;   ///< walk bound from SchemaLayout
};

/// Plan the probe for one field given the class-info address and the
/// declared field count. Returns nullopt when the count exceeds the bound
/// (corrupt or misunderstood table — never walk unbounded).
inline std::optional<FieldProbe> plan_field_probe(
    const SchemaLayout& layout,
    std::uint64_t class_info_address,
    std::uint32_t declared_field_count,
    const char* field_name) {
  if (declared_field_count > layout.max_fields_per_class) return std::nullopt;
  FieldProbe probe;
  probe.class_info_address = class_info_address;
  probe.field_table_address = class_info_address + layout.class_field_table_offset;
  probe.field_count = declared_field_count;
  probe.name_hash = fnv1a(field_name);
  probe.max_fields = layout.max_fields_per_class;
  return probe;
}

/// Does one walked field entry match the wanted hash (read its name pointer
/// and compare hashes out-of-band)? The walker reads the entry's name
/// pointer at ``entry + field_name_offset``, dereferences, hashes, and calls
/// this with the result — keeping the decision logic pure.
inline bool entry_matches(const FieldProbe& probe,
                          std::uint32_t walked_name_hash) {
  return walked_name_hash == probe.name_hash && probe.name_hash != 0;
}

// ── Read-driven field-table walk (the tier-2 core) ──────────────────
//
// Everything below is parameterized by a read functor
// ``bool(std::uint64_t address, void* dst, std::size_t size)`` so the walk
// is host-testable against a synthetic schema image and reusable by any
// Windows backend. What remains Windows-specific (discovering the
// SchemaSystem_001 instance and the CSchemaClassInfo addresses — interface
// resolution / pattern work, cf. strategy pair E22) stays in the adapters;
// given a class-info address this walk yields the field offset with no
// hardcoded schema offsets beyond the layout constants above.

/// Read one trivially-copyable scalar through ``read``.
template <typename T, typename ReadFn>
bool read_pod(ReadFn read, std::uint64_t address, T& out) {
  return read(address, &out, sizeof(T));
}

/// Read a NUL-terminated string through ``read`` and return its FNV-1a
/// hash. Returns nullopt when the read fails or the bound is exhausted
/// without a terminator (corrupt pointer).
template <typename ReadFn>
std::optional<std::uint32_t> hash_cstring_at(ReadFn read, std::uint64_t address,
                                             std::size_t max_len) {
  if (address == 0 || max_len == 0) return std::nullopt;
  std::uint32_t hash = kFnvOffsetBasis;
  bool terminated = false;
  for (std::size_t i = 0; i < max_len; ++i) {
    char c = 0;
    if (!read(address + i, &c, 1)) return std::nullopt;
    if (c == '\0') {
      terminated = true;
      break;
    }
    hash ^= static_cast<std::uint8_t>(c);
    hash *= kFnvPrime;
  }
  if (!terminated) return std::nullopt;
  return hash;
}

/// Walk a declared-class field table looking for one field by name hash.
/// Returns the field's offset, nullopt when absent/unreadable. Bounded by
/// ``probe.field_count`` and ``probe.max_fields``.
template <typename ReadFn>
std::optional<std::uint64_t> walk_field_table(ReadFn read,
                                              const FieldProbe& probe,
                                              const SchemaLayout& layout) {
  if (probe.field_count == 0 || probe.field_count > probe.max_fields) {
    return std::nullopt;
  }
  for (std::uint32_t index = 0; index < probe.field_count; ++index) {
    const std::uint64_t entry =
        probe.field_table_address + static_cast<std::uint64_t>(index) * layout.field_stride;
    std::uint64_t name_pointer = 0;
    if (!read_pod(read, entry + layout.field_name_offset, name_pointer) ||
        name_pointer == 0) {
      continue;
    }
    const auto name_hash =
        hash_cstring_at(read, name_pointer, layout.max_name_length);
    if (!name_hash || !entry_matches(probe, *name_hash)) continue;
    std::uint32_t field_offset = 0;
    if (!read_pod(read, entry + layout.field_offset_offset, field_offset)) {
      continue;
    }
    return static_cast<std::uint64_t>(field_offset);
  }
  return std::nullopt;
}

}  // namespace real::cs2::schema
