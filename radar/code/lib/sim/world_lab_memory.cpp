// world.cpp — sim::World implementation: processes, handles, drivers, net scars.
// make_arena() seeds the educational OS/game snapshot used by all pairs.

#include "sim/world.hpp"

#include <algorithm>
#include <cstring>

namespace sim {

namespace {

constexpr std::size_t kEntRawSize = 16;  // float*3 + team + alive + pad[2]
constexpr std::size_t kMinLabMem = 0x400;

void write_entity_table(std::vector<std::uint8_t>& mem, std::uint32_t table_rel) {
  const std::size_t need = static_cast<std::size_t>(table_rel) + 0x10 + 2 * kEntRawSize;
  if (mem.size() < need) {
    mem.resize(need, 0);
  }
  // Clear prior table region lightly when relocating (zero count at old default).
  if (table_rel != 0 && mem.size() >= 4) {
    std::memset(mem.data(), 0, 4);
  }
  const std::uint32_t count = 2;
  std::memcpy(mem.data() + table_rel, &count, sizeof(count));
  struct Ent {
    float x, y, z;
    std::uint8_t team, alive, pad[2];
  } e0{10.f, 0.f, 20.f, 1, 1, {}}, e1{50.f, 0.f, 80.f, 2, 1, {}};
  std::memcpy(mem.data() + table_rel + 0x10, &e0, sizeof(e0));
  std::memcpy(mem.data() + table_rel + 0x10 + sizeof(Ent), &e1, sizeof(e1));
}

void write_acpt_marker(std::vector<std::uint8_t>& mem, std::size_t marker_off,
                       std::uint32_t table_rel) {
  if (mem.size() < marker_off + 8) {
    mem.resize(marker_off + 8, 0);
  }
  mem[marker_off + 0] = 'A';
  mem[marker_off + 1] = 'C';
  mem[marker_off + 2] = 'P';
  mem[marker_off + 3] = 'T';
  std::memcpy(mem.data() + marker_off + 4, &table_rel, sizeof(table_rel));
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
  if (g->memory.size() < kMinLabMem) {
    g->memory.resize(kMinLabMem, 0);
  }
  lab_entity_table_rel = 0;
  lab_pattern_marker_off = 0x200;
  write_entity_table(g->memory, lab_entity_table_rel);
  write_acpt_marker(g->memory, lab_pattern_marker_off, lab_entity_table_rel);
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
  const std::size_t table_end =
      static_cast<std::size_t>(new_table_rel) + 0x10 + 2 * kEntRawSize;
  if (new_marker_off < table_end && new_marker_off + 8 > new_table_rel) {
    return false;
  }
  const std::size_t need =
      std::max(new_marker_off + 8, table_end);
  if (g->memory.size() < need) {
    g->memory.resize(need, 0);
  }
  // Wipe old marker if present.
  if (lab_pattern_marker_present &&
      lab_pattern_marker_off + 8 <= g->memory.size()) {
    std::memset(g->memory.data() + lab_pattern_marker_off, 0, 8);
  }
  lab_pattern_marker_off = new_marker_off;
  lab_entity_table_rel = new_table_rel;
  write_entity_table(g->memory, lab_entity_table_rel);
  write_acpt_marker(g->memory, lab_pattern_marker_off, lab_entity_table_rel);
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
  constexpr std::size_t kEnt = 16;
  const std::size_t need =
      static_cast<std::size_t>(table_rel) + 0x10 + entities.size() * kEnt;
  if (g->memory.size() < need) {
    g->memory.resize(need, 0);
  }
  const std::uint32_t count = static_cast<std::uint32_t>(entities.size());
  std::memcpy(g->memory.data() + table_rel, &count, sizeof(count));
  for (std::size_t i = 0; i < entities.size(); ++i) {
    const auto& e = entities[i];
    struct Ent {
      float x, y, z;
      std::uint8_t team, alive, pad[2];
    } row{e.origin.x, e.origin.y, e.origin.z, e.team,
          static_cast<std::uint8_t>(e.alive ? 1 : 0), {}};
    std::memcpy(g->memory.data() + table_rel + 0x10 + i * kEnt, &row,
                sizeof(row));
  }
  lab_entity_table_rel = table_rel;
  // Keep ACPT marker pointing at this table for pattern-scan paths.
  if (lab_pattern_marker_off + 8 > g->memory.size()) {
    g->memory.resize(lab_pattern_marker_off + 8, 0);
  }
  g->memory[lab_pattern_marker_off + 0] = 'A';
  g->memory[lab_pattern_marker_off + 1] = 'C';
  g->memory[lab_pattern_marker_off + 2] = 'P';
  g->memory[lab_pattern_marker_off + 3] = 'T';
  std::memcpy(g->memory.data() + lab_pattern_marker_off + 4, &table_rel,
              sizeof(table_rel));
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
  if (g->memory.size() < static_cast<std::size_t>(rel) + 4) {
    return out;
  }
  std::uint32_t count = 0;
  std::memcpy(&count, g->memory.data() + rel, sizeof(count));
  constexpr std::size_t kEnt = 16;
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::size_t off =
        static_cast<std::size_t>(rel) + 0x10 + static_cast<std::size_t>(i) * kEnt;
    if (off + kEnt > g->memory.size()) {
      break;
    }
    struct Ent {
      float x, y, z;
      std::uint8_t team, alive, pad[2];
    } row{};
    std::memcpy(&row, g->memory.data() + off, sizeof(row));
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
  if (addr < t->base ||
      static_cast<std::size_t>(addr - t->base) + size > t->memory.size()) {
    out.status = ac::Status::InvalidArgument;
    return out;
  }
  const auto off = static_cast<std::size_t>(addr - t->base);
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
