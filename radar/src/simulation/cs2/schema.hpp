#pragma once

// Educational CS2 schema field resolver (FNV-1a name hash + known field table).
// Models the Source-2 schema system used by live offset dumps without host PE
// parsing. Strategy pairs and unit tests exercise this pure table path.

#include "cs2/offsets.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace cs2 {

// FNV-1a 32-bit as used by Source-2 schema field hashing.
inline constexpr std::uint32_t kFnv1aOffsetBasis = 2166136261u;
inline constexpr std::uint32_t kFnv1aPrime = 16777619u;

constexpr std::uint32_t fnv1a32(std::string_view text) noexcept {
  std::uint32_t hash = kFnv1aOffsetBasis;
  for (unsigned char ch : text) {
    hash ^= ch;
    hash *= kFnv1aPrime;
  }
  return hash;
}

struct SchemaField {
  std::string_view class_name;
  std::string_view field_name;
  std::uint32_t name_hash = 0;
  int offset = 0;
  std::string_view type_name;
};

struct SchemaClass {
  std::string_view name;
  int field_count = 0;
  const SchemaField* fields = nullptr;
};

// Compile-time / static registry of high-value schema fields for the radar
// and diagnostic path. Offsets track FieldOffsets defaults.
struct SchemaRegistry {
  static constexpr int kClassCount = 6;

  static const SchemaRegistry& get();

  const SchemaField* find(std::string_view class_name,
                          std::string_view field_name) const;
  const SchemaField* find_by_hash(std::string_view class_name,
                                  std::uint32_t field_hash) const;
  int resolve_offset(std::string_view class_name,
                     std::string_view field_name) const;
  int resolve_offset_by_hash(std::string_view class_name,
                             std::uint32_t field_hash) const;
  const SchemaClass* find_class(std::string_view class_name) const;
  int total_fields() const;
  std::vector<SchemaField> all_fields() const;

  SchemaClass classes[kClassCount]{};
};

// Apply resolved schema offsets onto a FieldOffsets instance.
// Returns the number of fields applied.
int apply_schema_to_field_offsets(FieldOffsets& out);

}  // namespace cs2
