// world.cpp — sim::World implementation: processes, handles, drivers, net scars.
// make_arena() seeds the educational OS/game snapshot used by all pairs.

#include "sim/world.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace sim {

namespace {

constexpr std::size_t kEntRawSize = 16;  // float*3 + team + alive + pad[2]
constexpr std::size_t kMinLabMem = 0x400;
constexpr std::size_t kEntityRowsOffset = 0x10;
constexpr std::size_t kMarkerSize = 8;

struct LabEntityRow {
  float x, y, z;
  std::uint8_t team, alive, pad[2];
};
static_assert(std::is_trivially_copyable_v<LabEntityRow>);
static_assert(sizeof(LabEntityRow) == kEntRawSize);

bool checked_add(std::size_t left, std::size_t right, std::size_t& out) {
  if (right > std::numeric_limits<std::size_t>::max() - left) {
    return false;
  }
  out = left + right;
  return true;
}

bool checked_mul(std::size_t left, std::size_t right, std::size_t& out) {
  if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
    return false;
  }
  out = left * right;
  return true;
}

bool range_fits(std::size_t offset, std::size_t size, std::size_t extent) {
  return offset <= extent && size <= extent - offset;
}

bool address_offset(std::uint64_t address, std::uint64_t base,
                    std::size_t& offset) {
  if (address < base) {
    return false;
  }
  const auto difference = address - base;
  if (difference > std::numeric_limits<std::size_t>::max()) {
    return false;
  }
  offset = static_cast<std::size_t>(difference);
  return true;
}

bool ensure_extent(std::vector<std::uint8_t>& memory, std::size_t offset,
                   std::size_t size) {
  std::size_t end = 0;
  if (!checked_add(offset, size, end) || end > memory.max_size()) {
    return false;
  }
  if (memory.size() < end) {
    memory.resize(end, 0);
  }
  return true;
}

template <typename T>
bool write_object(std::vector<std::uint8_t>& memory, std::size_t offset,
                  const T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  if (!range_fits(offset, sizeof(T), memory.size())) {
    return false;
  }
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
  std::copy_n(bytes, sizeof(T),
              memory.begin() + static_cast<std::ptrdiff_t>(offset));
  return true;
}

template <typename T>
bool read_object(const std::vector<std::uint8_t>& memory, std::size_t offset,
                 T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  if (!range_fits(offset, sizeof(T), memory.size())) {
    return false;
  }
  auto* bytes = reinterpret_cast<std::uint8_t*>(&value);
  std::copy_n(memory.begin() + static_cast<std::ptrdiff_t>(offset), sizeof(T),
              bytes);
  return true;
}

bool entity_table_end(std::uint32_t table_rel, std::size_t row_count,
                      std::size_t& end) {
  std::size_t rows_size = 0;
  std::size_t rows_offset = 0;
  return checked_mul(row_count, kEntRawSize, rows_size) &&
         checked_add(static_cast<std::size_t>(table_rel), kEntityRowsOffset,
                     rows_offset) &&
         checked_add(rows_offset, rows_size, end);
}

bool write_entity_table(std::vector<std::uint8_t>& mem, std::uint32_t table_rel) {
  std::size_t end = 0;
  if (!entity_table_end(table_rel, 2, end) ||
      !ensure_extent(mem, static_cast<std::size_t>(table_rel),
                     end - static_cast<std::size_t>(table_rel))) {
    return false;
  }
  // Clear prior table region lightly when relocating (zero count at old default).
  if (table_rel != 0 && range_fits(0, sizeof(std::uint32_t), mem.size())) {
    std::fill_n(mem.begin(), sizeof(std::uint32_t), 0);
  }
  const std::uint32_t count = 2;
  const LabEntityRow e0{10.f, 0.f, 20.f, 1, 1, {}};
  const LabEntityRow e1{50.f, 0.f, 80.f, 2, 1, {}};
  const auto rows_offset = static_cast<std::size_t>(table_rel) + kEntityRowsOffset;
  return write_object(mem, table_rel, count) &&
         write_object(mem, rows_offset, e0) &&
         write_object(mem, rows_offset + sizeof(e0), e1);
}

bool write_acpt_marker(std::vector<std::uint8_t>& mem, std::size_t marker_off,
                       std::uint32_t table_rel) {
  if (!ensure_extent(mem, marker_off, kMarkerSize)) {
    return false;
  }
  mem[marker_off + 0] = 'A';
  mem[marker_off + 1] = 'C';
  mem[marker_off + 2] = 'P';
  mem[marker_off + 3] = 'T';
  return write_object(mem, marker_off + 4, table_rel);
}

}  // namespace

// World::plant_lab_entities: Seed game memory with synthetic entity table for RPM labs.
// Also plants a lab AOB marker (magic "ACPT" + table relative offset) so pattern-scan
// red can discover the table without hard-coding offsets ().
void World::plant_lab_entities(std::uint32_t gpid) {
  auto* g = proc(gpid);
  if (!g || !g->is_game) {
    return;
  }
  if (!ensure_extent(g->memory, 0, kMinLabMem)) {
    return;
  }
  lab_entity_table_rel = 0;
  lab_pattern_marker_off = 0x200;
  if (!write_entity_table(g->memory, lab_entity_table_rel) ||
      !write_acpt_marker(g->memory, lab_pattern_marker_off,
                         lab_entity_table_rel)) {
    return;
  }
  lab_pattern_marker_present = true;
  ++lab_pattern_generation;

  note("planted lab entities + ACPT pattern marker gen=" +
       std::to_string(lab_pattern_generation) + " pid=" + std::to_string(gpid));
}

// World::mutate_lab_pattern_layout: Move ACPT marker and/or entity table (refresh lesson).
bool World::mutate_lab_pattern_layout(std::uint32_t gpid, std::size_t new_marker_off,
                                      std::uint32_t new_table_rel) {
  auto* g = proc(gpid);
  if (!g || !g->is_game) {
    return false;
  }
  if (new_marker_off < 8 || new_table_rel > 0x300) {
    return false;
  }
  // Avoid overlapping marker and entity table rows.
  std::size_t table_end = 0;
  std::size_t marker_end = 0;
  if (!entity_table_end(new_table_rel, 2, table_end) ||
      !checked_add(new_marker_off, kMarkerSize, marker_end)) {
    return false;
  }
  if (new_marker_off < table_end && marker_end > new_table_rel) {
    return false;
  }
  const std::size_t need = std::max(marker_end, table_end);
  if (!ensure_extent(g->memory, 0, need)) {
    return false;
  }
  // Wipe old marker if present.
  if (lab_pattern_marker_present &&
      range_fits(lab_pattern_marker_off, kMarkerSize, g->memory.size())) {
    std::fill_n(g->memory.begin() +
                    static_cast<std::ptrdiff_t>(lab_pattern_marker_off),
                kMarkerSize, 0);
  }
  lab_pattern_marker_off = new_marker_off;
  lab_entity_table_rel = new_table_rel;
  if (!write_entity_table(g->memory, lab_entity_table_rel) ||
      !write_acpt_marker(g->memory, lab_pattern_marker_off,
                         lab_entity_table_rel)) {
    return false;
  }
  lab_pattern_marker_present = true;
  ++lab_pattern_generation;
  note("mutate_lab_pattern_layout marker_off=" + std::to_string(new_marker_off) +
       " table_rel=" + std::to_string(new_table_rel) +
       " gen=" + std::to_string(lab_pattern_generation));
  return true;
}

// World::open_process: Record a handle from owner→target with AccessMask (scar).
bool World::plant_entity_snapshots(
    std::uint32_t gpid, const std::vector<ac::EntitySnapshot>& entities,
    std::uint32_t table_rel) {
  auto* g = proc(gpid);
  if (!g || !g->is_game) {
    return false;
  }
  if (entities.size() > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  std::size_t table_end = 0;
  if (!entity_table_end(table_rel, entities.size(), table_end) ||
      !ensure_extent(g->memory, static_cast<std::size_t>(table_rel),
                     table_end - static_cast<std::size_t>(table_rel)) ||
      !ensure_extent(g->memory, lab_pattern_marker_off, kMarkerSize)) {
    return false;
  }
  const std::uint32_t count = static_cast<std::uint32_t>(entities.size());
  if (!write_object(g->memory, table_rel, count)) {
    return false;
  }
  for (std::size_t i = 0; i < entities.size(); ++i) {
    const auto& e = entities[i];
    const LabEntityRow row{e.origin.x, e.origin.y, e.origin.z, e.team,
                           static_cast<std::uint8_t>(e.alive ? 1 : 0), {}};
    std::size_t row_delta = 0;
    std::size_t row_offset = 0;
    if (!checked_mul(i, sizeof(row), row_delta) ||
        !checked_add(static_cast<std::size_t>(table_rel) + kEntityRowsOffset,
                     row_delta, row_offset) ||
        !write_object(g->memory, row_offset, row)) {
      return false;
    }
  }
  lab_entity_table_rel = table_rel;
  // Keep ACPT marker pointing at this table for pattern-scan paths.
  if (!write_acpt_marker(g->memory, lab_pattern_marker_off, table_rel)) {
    return false;
  }
  lab_pattern_marker_present = true;
  ++lab_pattern_generation;
  note("plant_entity_snapshots count=" + std::to_string(count) +
       " gen=" + std::to_string(lab_pattern_generation));
  return true;
}

// World::read_entity_snapshots: Decode planted entity table.
std::vector<ac::EntitySnapshot> World::read_entity_snapshots(
    std::uint32_t gpid, std::uint32_t table_rel) const {
  std::vector<ac::EntitySnapshot> out;
  const auto* g = proc(gpid);
  if (!g || !g->is_game) {
    return out;
  }
  const auto rel = table_rel != 0 ? table_rel : lab_entity_table_rel;
  if (!range_fits(rel, sizeof(std::uint32_t), g->memory.size())) {
    return out;
  }
  std::uint32_t count = 0;
  if (!read_object(g->memory, rel, count)) {
    return out;
  }
  std::size_t rows_offset = 0;
  if (!checked_add(static_cast<std::size_t>(rel), kEntityRowsOffset,
                   rows_offset)) {
    return out;
  }
  const auto available_rows = rows_offset <= g->memory.size()
                                  ? (g->memory.size() - rows_offset) / kEntRawSize
                                  : 0;
  if (count > available_rows) {
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::size_t row_delta = 0;
    std::size_t off = 0;
    LabEntityRow row{};
    if (!checked_mul(i, sizeof(row), row_delta) ||
        !checked_add(rows_offset, row_delta, off) ||
        !read_object(g->memory, off, row)) {
      return {};
    }
    out.emplace_back(i + 1, ac::Vec3{row.x, row.y, row.z}, row.team,
                     row.alive != 0);
  }
  return out;
}

// World::read_mem: Record a read_mem operation; may copy lab bytes into out buffer.
ac::ReadResult World::read_mem(std::uint32_t reader_pid, std::uint32_t target_pid,
                               std::uint64_t addr, std::size_t size,
                               bool require_handle) {
  ac::ReadResult out;
  auto* t = proc(target_pid);
  if (!t) {
    out.status = ac::Status::Unavailable;
    return out;
  }
  if (require_handle) {
    bool ok = false;
    for (const auto& h : handles) {
      if (h.owner_pid == reader_pid && h.target_pid == target_pid &&
          has(h.access, AccessMask::VmRead)) {
        ok = true;
        break;
      }
    }
    if (!ok) {
      out.status = ac::Status::Denied;
      return out;
    }
  }
  std::size_t off = 0;
  if (!address_offset(addr, t->base, off) ||
      !range_fits(off, size, t->memory.size())) {
    out.status = ac::Status::InvalidArgument;
    return out;
  }
  out.bytes.assign(t->memory.begin() + static_cast<std::ptrdiff_t>(off),
                   t->memory.begin() + static_cast<std::ptrdiff_t>(off + size));
  out.status = ac::Status::Ok;
  // Bulk/pattern-scan residual: count successful handle-gated remote reads.
  if (require_handle) {
    ++remote_read_ops;
    remote_read_bytes += static_cast<std::uint32_t>(size);
  }
  return out;
}

// World::load_driver: Install a Driver residual (BYOVD/memrw flags for blue).

}  // namespace sim
