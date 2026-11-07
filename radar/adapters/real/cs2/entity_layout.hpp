// entity_layout.hpp: Pure Source-2 entity-list layout math and bulk-read
// planning (assessment ledger E11 / E42).
//
// Everything here is platform-neutral and host-testable: no Windows headers,
// no I/O, no Result type. The Windows walkers (entities.cpp,
// live_radar_stack_entity.cpp) use these helpers to collapse the per-player
// read storm into batched transfers:
//
//   before: ~2 reads per controller resolve (chunk ptr + slot) x N controllers
//           + ~8 small reads per pawn (team/health/lifestate/node/origin/...)
//   after:  1 read of the whole first controller page (N slots) + 1 bulk
//           pawn-window read per live pawn (+1-2 scene-node reads).
//
// The bounds-checked extraction below exists because of the silent-degradation
// failure mode documented in the assessment ledger (E42): an out-of-window
// schema offset that quietly yields zeros. Here an out-of-window access is a
// hard, typed error, never silent.
//
// Educational framing: bulk reads trade a larger single observable transfer
// for far fewer observable transactions. Blue sees read *volume* (roughly
// unchanged) but far fewer read *events*; both belong in the red/blue cost
// model (strategy pair E25).

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace real::cs2::layout {

/// Offset+size of one field inside a bulk-read window.
struct FieldSpan {
  std::uint64_t offset = 0;
  std::size_t size = 0;
};

/// A byte window read in one transaction (e.g. a pawn head or a controller
/// page), with bounds-checked field extraction.
class Window {
 public:
  Window() = default;
  explicit Window(std::vector<std::uint8_t> bytes): bytes_(std::move(bytes)) {}

  bool valid() const { return !bytes_.empty(); }
  std::size_t size() const { return bytes_.size(); }
  const std::uint8_t* data() const { return bytes_.data(); }

  /// Bounds-checked field extraction. Returns false (and zeroes ``out``)
  /// when the field span is not fully inside the window, never silent.
  template <typename T>
  bool extract(FieldSpan span, T& out) const {
    if (span.size != sizeof(T)) return false;
    if (span.offset > bytes_.size() || sizeof(T) > bytes_.size() - span.offset) {
      return false;
    }
    std::memcpy(&out, bytes_.data() + span.offset, sizeof(T));
    return true;
  }

  /// Raw pointer to a byte range, bounds-checked.
  const std::uint8_t* span_pointer(FieldSpan span) const {
    if (span.offset > bytes_.size() || span.size > bytes_.size() - span.offset) {
      return nullptr;
    }
    return bytes_.data() + span.offset;
  }

  /// Mutable access for the producer that fills the window in one
  /// transaction before any extraction happens.
  std::uint8_t* data_mutable() { return bytes_.data(); }

 private:
  std::vector<std::uint8_t> bytes_;
};

/// Hard cap on any single bulk window (64 KiB, matches the donor IPC
/// payload and typical METHOD_BUFFERED limits). Fields beyond the cap cannot
/// be served from the bulk window; callers either read them directly or the
/// bounds-checked extraction fails loudly (never silently).
inline constexpr std::size_t kMaxWindowBytes = 0x10000;

/// Smallest window that fully contains every field span that fits under
/// ``kMaxWindowBytes`` (0 when none fit). Oversize fields are silently
/// skipped; use the strict variant when the caller must know (E65 / S2).
inline std::size_t required_window_size(const std::vector<FieldSpan>& fields) {
  std::size_t end = 0;
  for (const FieldSpan& f : fields) {
    const std::uint64_t field_end = f.offset + f.size;
    if (f.offset >= kMaxWindowBytes || field_end > kMaxWindowBytes) continue;
    if (field_end > end) end = static_cast<std::size_t>(field_end);
  }
  return end;
}

/// Strict variant: ``std::nullopt`` when any field does not fit under the
/// window cap. The caller then knows the window is incomplete instead of
/// discovering it later as a failed extraction.
inline std::optional<std::size_t> required_window_size_strict(
    const std::vector<FieldSpan>& fields) {
  std::size_t end = 0;
  for (const FieldSpan& f : fields) {
    const std::uint64_t field_end = f.offset + f.size;
    if (f.offset >= kMaxWindowBytes || field_end > kMaxWindowBytes) {
      return std::nullopt;
    }
    if (field_end > end) end = static_cast<std::size_t>(field_end);
  }
  return end;
}

/// Source-2 chunked identity-list constants (defaults; the runtime snapshot
/// may override the stride).
struct ListGeometry {
  std::uint32_t handle_index_mask = 0x7FFFu;
  std::uint32_t chunk_shift = 9;
  std::uint64_t chunk_table_offset = 0x10;
  std::uint64_t stride = 112;  // CEntityIdentity slot stride (0x70)
};

inline std::uint32_t handle_index(std::uint32_t handle, const ListGeometry& g) {
  return handle & g.handle_index_mask;
}

/// Address of the chunk-pointer slot for ``handle``'s index.
inline std::uint64_t chunk_pointer_address(std::uint64_t entity_list,
                                           std::uint32_t handle,
                                           const ListGeometry& g) {
  const std::uint64_t chunk =
      static_cast<std::uint64_t>(handle_index(handle, g) >> g.chunk_shift);
  return entity_list + g.chunk_table_offset + chunk * sizeof(std::uint64_t);
}

/// Address of the identity slot for ``index`` inside chunk ``chunk_base``.
inline std::uint64_t slot_address(std::uint64_t chunk_base,
                                  std::uint32_t index,
                                  const ListGeometry& g) {
  const std::uint32_t in_chunk = index & ((1u << g.chunk_shift) - 1u);
  return chunk_base + static_cast<std::uint64_t>(in_chunk) * g.stride;
}

/// One contiguous single-read batch: read ``[address, address+size)`` once
/// and resolve every covered identity slot from the buffer.
struct BatchPlan {
  std::uint64_t address = 0;
  std::size_t size = 0;
  std::uint32_t first_index = 0;  ///< first identity index covered
  std::uint32_t count = 0;        ///< number of identity slots covered

  /// Offset of identity slot ``index`` inside the batch (bytes), or npos.
  static constexpr std::size_t npos = static_cast<std::size_t>(-1);
  std::size_t slot_offset(std::uint32_t index, const ListGeometry& g) const {
    if (count == 0 || index < first_index || index >= first_index + count) {
      return npos;
    }
    return static_cast<std::size_t>(index - first_index) * g.stride;
  }
};

/// Plan one single-read batch covering identity indices
/// ``[first, first+count)`` (all indices must live in the same chunk).
inline BatchPlan plan_controller_batch(std::uint64_t chunk_base,
                                       std::uint32_t first_index,
                                       std::uint32_t count,
                                       const ListGeometry& g) {
  BatchPlan plan;
  if (count == 0) return plan;
  const std::uint32_t in_chunk = first_index & ((1u << g.chunk_shift) - 1u);
  // A batch may never cross a chunk boundary.
  const std::uint32_t room = (1u << g.chunk_shift) - in_chunk;
  plan.first_index = first_index;
  plan.count = count < room ? count: room;
  plan.address = chunk_base + static_cast<std::uint64_t>(in_chunk) * g.stride;
  plan.size = static_cast<std::size_t>(plan.count) * static_cast<std::size_t>(g.stride);
  return plan;
}

/// Plausibility gates applied to pawn fields before an entity is emitted
/// (ledger E7 analog on the memory path: degrade to "don't emit" instead of
/// emitting garbage). The gates are deliberately loose; they catch torn or
/// misaligned reads, not legitimate play states.
struct PawnSanity {
  static constexpr std::int32_t kMaxHealth = 500;
  static constexpr std::uint32_t kTeamMask = 0xFFu;
  static constexpr std::uint8_t kLifeStateAlive = 0;

  static bool health(std::int32_t value) {
    return value > 0 && value <= kMaxHealth;
  }
  static bool team(std::uint32_t raw) {
    return (raw & ~kTeamMask) == 0;
  }
  static bool alive(std::int32_t health_value, std::uint8_t life_state) {
    return health_value > 0 && life_state == kLifeStateAlive;
  }
};

}  // namespace real::cs2::layout
