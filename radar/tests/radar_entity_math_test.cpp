// radar_entity_math_test.cpp: Host tests for the pure: platform-neutral
// headers introduced by the external-repo assessment ledger:
//
//   E11 entity_layout.hpp, bulk-read window math, bounds-checked
//                            extraction, batch planning, sanity gates
//   E12 offsets_validate.hpp, functional offset validation classifier
//   E14 entity_cache.hpp, event-diff cache + torn-free triple buffer
//   E13 schema_layout.hpp, FNV-1a, schema walk plan, bounded memo
//   E15 frame_pacing.hpp, sleep/spin split, idle debounce
//   E16 eprocess_derive.hpp, EPROCESS offset derivation over a synthetic
//                            memory image (full derivation + list walk)
//
// Nothing here links a real adapter or touches a live process.

#include "real/cs2/entity_cache.hpp"
#include "real/cs2/entity_layout.hpp"
#include "real/cs2/frame_pacing.hpp"
#include "real/cs2/offsets_validate.hpp"
#include "real/cs2/schema_layout.hpp"
#include "real/kernel/ioctl_interface.hpp"
#include "real/kernel/eprocess_derive.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ── tiny assertion helpers (portable test, no framework) ───────────
namespace {

int failures = 0;

#define CHECK(condition)                                                    \
  do {                                                                      \
    if (!(condition)) {                                                     \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);      \
      ++failures;                                                           \
    }                                                                       \
  } while (false)

#define CHECK_EQ(a, b)                                                      \
  do {                                                                      \
    if (!((a) == (b))) {                                                    \
      std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);    \
      ++failures;                                                           \
    }                                                                       \
  } while (false)

// ── synthetic memory image for the EPROCESS derivation ─────────────
struct FakeMemory {
  std::map<std::uint64_t, std::vector<std::uint8_t>> pages;

  void write(std::uint64_t address, const void* data, std::size_t size) {
    auto& page = pages[address];
    page.assign(static_cast<const std::uint8_t*>(data),
                static_cast<const std::uint8_t*>(data) + size);
  }

  bool read(std::uint64_t address, void* dst, std::size_t size) const {
    std::size_t copied = 0;
    while (copied < size) {
      bool found = false;
      for (const auto& [start, bytes]: pages) {
        const std::uint64_t at = address + copied;
        if (at >= start && at < start + bytes.size()) {
          const std::size_t offset = static_cast<std::size_t>(at - start);
          const std::size_t chunk =
              std::min(size - copied, bytes.size() - offset);
          std::memcpy(static_cast<std::uint8_t*>(dst) + copied,
                      bytes.data() + offset, chunk);
          copied += chunk;
          found = true;
          break;
        }
      }
      if (!found) return false;
    }
    return true;
  }
};

}  // namespace

// ── E11: entity layout ─────────────────────────────────────────────

static void test_window_bounds_checked_extraction() {
  using namespace real::cs2::layout;
  std::vector<std::uint8_t> bytes(64, 0);
  const std::uint32_t value = 0xCAFEBABE;
  std::memcpy(bytes.data() + 32, &value, sizeof(value));
  const Window window(bytes);

  std::uint32_t out = 0;
  CHECK(window.extract<std::uint32_t>({32, 4}, out));
  CHECK_EQ(out, 0xCAFEBABEu);

  // Out of window: hard failure, never silent zeros.
  out = 0;
  CHECK(!window.extract<std::uint32_t>({62, 4}, out));
  CHECK(!window.extract<std::uint32_t>({64, 4}, out));
  CHECK(!window.extract<std::uint32_t>({32, 8}, out));  // size mismatch
  CHECK_EQ(out, 0u);
  CHECK(window.span_pointer({60, 4}) != nullptr);
  CHECK(window.span_pointer({60, 5}) == nullptr);
}

static void test_required_window_size_and_cap() {
  using namespace real::cs2::layout;
  CHECK_EQ(required_window_size({}), 0u);
  CHECK_EQ(required_window_size({{0x10, 4}, {0x20, 8}}), 0x28u);
  // A field beyond the cap is skipped; the window only covers the rest.
  CHECK_EQ(required_window_size({{0x10, 4}, {0x20000, 4}}), 0x14u);
}

static void test_chunk_math_and_batch_plan() {
  using namespace real::cs2::layout;
  const ListGeometry g;
  CHECK_EQ(handle_index(0x1234, g), 0x1234u);
  CHECK_EQ(handle_index(0x8000u | 0x234, g), 0x234u);  // serial bits masked
  const std::uint64_t expected_chunk = 0x1234u >> 9;
  CHECK_EQ(chunk_pointer_address(0x1000'0000, 0x1234, g),
           0x1000'0000ull + 0x10ull + expected_chunk * 8ull);

  // Slots 1..64 all live in chunk 0: one batch covers them.
  const BatchPlan plan = plan_controller_batch(0x2000'0000, 1, 64, g);
  CHECK_EQ(plan.address, 0x2000'0000ull + 1 * g.stride);
  CHECK_EQ(plan.size, 64u * g.stride);
  CHECK_EQ(plan.slot_offset(1, g), 0u);
  CHECK_EQ(plan.slot_offset(64, g), 63u * g.stride);
  CHECK_EQ(plan.slot_offset(0, g), BatchPlan::npos);
  CHECK_EQ(plan.slot_offset(65, g), BatchPlan::npos);

  // A batch at the chunk boundary is clamped to the chunk end.
  const BatchPlan clamped = plan_controller_batch(0x2000'0000, 500, 100, g);
  CHECK_EQ(clamped.count, (1u << 9) - 500);
  CHECK_EQ(clamped.size, ((1u << 9) - 500) * g.stride);
}

static void test_pawn_sanity_gates() {
  using namespace real::cs2::layout;
  CHECK(PawnSanity::health(100));
  CHECK(!PawnSanity::health(0));
  CHECK(!PawnSanity::health(100000));  // torn read signature
  CHECK(PawnSanity::team(2));
  CHECK(!PawnSanity::team(0x1FF));  // neighbour bleed
  CHECK(PawnSanity::alive(50, 0));
  CHECK(!PawnSanity::alive(50, 2));
}

// ── E12: offsets validation classifier ─────────────────────────────

static void test_offsets_validation_classification() {
  using namespace real::cs2::validate;
  Observation clean;
  clean.players_seen = 6;
  clean.pawns_resolved = 6;
  clean.plausible_fields = 24;
  clean.bogus_fields = 2;
  CHECK(classify(clean) == OffsetsValidation::Ok);

  Observation lobby;  // clean walk, nobody home
  lobby.players_seen = 0;
  CHECK(classify(lobby) == OffsetsValidation::NoPlayers);

  Observation unreadable;  // reads failing with zero resolutions
  unreadable.players_seen = 0;
  unreadable.failed_reads = 5;
  CHECK(classify(unreadable) == OffsetsValidation::Wrong);

  Observation bad_pawn_chain;  // controllers resolve, pawns never do
  bad_pawn_chain.players_seen = 4;
  bad_pawn_chain.pawns_resolved = 0;
  CHECK(classify(bad_pawn_chain) == OffsetsValidation::Wrong);

  Observation garbage_fields;  // mostly bogus values
  garbage_fields.players_seen = 4;
  garbage_fields.pawns_resolved = 4;
  garbage_fields.plausible_fields = 2;
  garbage_fields.bogus_fields = 30;
  CHECK(classify(garbage_fields) == OffsetsValidation::Wrong);

  CHECK(field_plausible(2, 100));
  CHECK(!field_plausible(9, 100));
  CHECK(!field_plausible(2, 5000));
}

// ── E14: entity cache + triple buffer ──────────────────────────────

static void test_triple_buffer_torn_free_under_rapid_publish() {
  using namespace real::cs2::cache;
  struct Payload {
    int frame = 0;
    std::uint64_t checksum = 0;
  };
  TripleBuffer<Payload> buffer;
  // Single writer, many publishes: a load that races a publish must either
  // see an intact older value or fail, never a torn hybrid.
  for (int frame = 0; frame < 1000; ++frame) {
    Payload p;
    p.frame = frame;
    p.checksum = static_cast<std::uint64_t>(frame) * 0x9E3779B97F4A7C15ull;
    buffer.publish(p);
    Payload out;
    if (buffer.load(out)) {
      // Whatever frame we observe, its checksum must match that frame.
      CHECK_EQ(out.checksum,
               static_cast<std::uint64_t>(out.frame) * 0x9E3779B97F4A7C15ull);
    }
  }
  Payload final{};
  CHECK(buffer.load(final));
  CHECK_EQ(final.frame, 999);
}

static void test_entity_cache_diffs_and_generations() {
  using namespace real::cs2::cache;
  struct Entity {
    int health = 0;
  };
  EntityCache<Entity> cache;
  cache.upsert(1, Entity{100});
  cache.upsert(2, Entity{80});
  auto snap = cache.publish();
  CHECK_EQ(snap.entities.size(), 2u);
  CHECK_EQ(cache.last_diff().added.size(), 2u);
  CHECK_EQ(cache.last_diff().removed.size(), 0u);

  cache.upsert(2, Entity{70});  // refresh, not an add
  cache.remove(1);
  snap = cache.publish();
  CHECK_EQ(snap.entities.size(), 1u);
  CHECK_EQ(snap.entities[0].health, 70);
  CHECK_EQ(cache.last_diff().added.size(), 0u);
  CHECK_EQ(cache.last_diff().removed.size(), 1u);

  // Generation grows on upsert; a snapshot is stale below it.
  const std::uint64_t before = cache.generation();
  cache.upsert(2, Entity{60});
  CHECK(cache.generation() > before);
  CHECK(snap.generation < cache.generation());
  snap = cache.publish();  // the new upsert must be published to be visible

  // Lock-free latest path agrees with the last publish.
  CacheSnapshot<Entity> latest{};
  CHECK(cache.latest(latest));
  CHECK_EQ(latest.entities.size(), 1u);
  CHECK_EQ(latest.entities[0].health, 60);
}

static void test_entity_cache_sync_full_walk() {
  using namespace real::cs2::cache;
  struct Entity {
    int health = 0;
    bool local = false;
  };
  EntityCache<Entity> cache;
  const auto key = [](const Entity& e) { return e.local ? 1u: e.health; };

  // Walk 1: two entities.
  std::vector<Entity> walk1 = {Entity{100, true}, Entity{80, false}};
  auto snap = cache.sync(walk1, key);
  CHECK_EQ(snap.entities.size(), 2u);
  CHECK_EQ(cache.last_diff().added.size(), 2u);
  CHECK_EQ(cache.last_diff().removed.size(), 0u);

  // Walk 2: one left, one joined, the absent handle is a removal event.
  std::vector<Entity> walk2 = {Entity{100, true}, Entity{55, false}};
  snap = cache.sync(walk2, key);
  CHECK_EQ(snap.entities.size(), 2u);
  CHECK_EQ(cache.last_diff().added.size(), 1u);
  CHECK_EQ(cache.last_diff().removed.size(), 1u);

  // Walk 3: same as walk 2, no diffs, generation still advanced by upserts.
  snap = cache.sync(walk2, key);
  CHECK_EQ(cache.last_diff().added.size(), 0u);
  CHECK_EQ(cache.last_diff().removed.size(), 0u);
  CHECK_EQ(snap.entities.size(), 2u);
}

// ── E13: schema layout ─────────────────────────────────────────────

static void test_schema_fnv1a_known_vectors() {
  using namespace real::cs2::schema;
  // FNV-1a 32-bit self-consistency + published empty-string vector.
  CHECK_EQ(fnv1a(""), 0x811C9DC5u);
  CHECK_EQ(fnv1a("a"), 0xE40C292Cu);
  // Distinct field names hash differently (collision-free for these).
  CHECK(fnv1a("m_iHealth") != fnv1a("m_iTeamNum"));
}

static void test_schema_field_probe_and_memo() {  using namespace real::cs2::schema;
  const SchemaLayout layout;
  auto probe = plan_field_probe(layout, 0xAAAA0000, 128, "m_iHealth");
  CHECK(probe.has_value());
  CHECK_EQ(probe->field_table_address, 0xAAAA0000ull + layout.class_field_table_offset);
  CHECK_EQ(probe->field_count, 128u);
  CHECK(entry_matches(*probe, fnv1a("m_iHealth")));
  CHECK(!entry_matches(*probe, fnv1a("m_iTeamNum")));

  // Bound: a corrupt declared count is rejected, never walked unbounded.
  CHECK(!plan_field_probe(layout, 0xAAAA0000, 100000, "m_iHealth").has_value());

  FieldMemo memo(4);
  CHECK(!memo.lookup("C_BaseEntity", "m_iHealth").has_value());
  memo.store("C_BaseEntity", "m_iHealth", 0x34C);
  CHECK_EQ(memo.lookup("C_BaseEntity", "m_iHealth").value(), 0x34Cull);
  CHECK_EQ(memo.lookup("C_BaseEntity", "m_iTeamNum").has_value(), false);
  // Capacity bound: overfill clears rather than growing unbounded.
  for (int i = 0; i < 10; ++i) {
    memo.store("C" + std::to_string(i), "f", static_cast<std::uint64_t>(i));
  }
  CHECK(memo.size() <= 4);
}

// ── E15: frame pacing ──────────────────────────────────────────────

static void test_frame_pacing_split() {
  using namespace real::cs2::pacing;
  using clock = std::chrono::steady_clock;
  PaceConfig config;  // 16667us active, 1500us spin window
  const auto start = clock::now();

  // Plenty of budget: sleep the gap minus the spin window.
  auto decision = pace_frame(start, start, config, false);
  CHECK(decision.paced);
  CHECK(!decision.idle);
  CHECK_EQ(decision.sleep_for.count(), 16667 - 1500);
  CHECK_EQ(decision.spin_for.count(), 1500);

  // Close to the deadline: all spin, no sleep.
  decision = pace_frame(start + std::chrono::microseconds(16000), start, config, false);
  CHECK(decision.sleep_for.count() == 0);
  CHECK_EQ(decision.spin_for.count(), 667);

  // Late: run immediately.
  decision = pace_frame(start + std::chrono::seconds(1), start, config, false);
  CHECK(decision.paced);
  CHECK_EQ(decision.sleep_for.count(), 0);
  CHECK_EQ(decision.spin_for.count(), 0);

  // Idle cap engaged.
  decision = pace_frame(start, start, config, true);
  CHECK(decision.idle);
  CHECK_EQ(decision.sleep_for.count(), 50000 - 1500);

  // Disabled periods.
  PaceConfig off{};
  off.active_period = std::chrono::microseconds(0);
  off.idle_period = std::chrono::microseconds(0);
  CHECK(!pace_frame(start, start, off, false).paced);
}

static void test_chunked_read_planner() {
  using namespace real::kernel;
  // Even split.
  auto plan = plan_chunked_read(0x1000, 1024, 512);
  CHECK_EQ(plan.size(), 2u);
  CHECK_EQ(plan[0].address, 0x1000ull);
  CHECK_EQ(plan[0].size, 512u);
  CHECK_EQ(plan[1].address, 0x1200ull);
  CHECK_EQ(plan[1].size, 512u);
  // Ragged tail.
  plan = plan_chunked_read(0x1000, 1025, 512);
  CHECK_EQ(plan.size(), 3u);
  CHECK_EQ(plan[2].size, 1u);
  // Zero total / zero chunk: empty plan.
  CHECK(plan_chunked_read(0x1000, 0, 512).empty());
  CHECK(plan_chunked_read(0x1000, 512, 0).empty());
  // Overflow: the range would wrap, refuse the whole read.
  CHECK(plan_chunked_read(0xFFFFFFFFFFFFFF00ull, 0x200, 0x100).empty());
  // Sanity: a normal range passes the overflow check.
  CHECK(chunk_range_ok(0x1000, 512, 512));
  CHECK(!chunk_range_ok(0xFFFFFFFFFFFFFFFFull, 8, 8));
}

static void test_idle_debouncer() {
  using namespace real::cs2::pacing;
  IdleDebouncer debouncer(3);
  CHECK(!debouncer.vote(true));
  CHECK(!debouncer.vote(true));
  CHECK(debouncer.vote(true));    // third consecutive idle vote engages
  CHECK(!debouncer.vote(false));  // any busy vote disengages immediately
  CHECK(!debouncer.idle());
  CHECK(!debouncer.vote(true));
  CHECK(!debouncer.vote(true));
  CHECK(debouncer.vote(true));
}

// ── E16: EPROCESS derivation over a synthetic image ────────────────

namespace {

// Build a synthetic kernel memory image: a System EPROCESS with a known
// layout, one neighbour process, page tables mapping a fake "ntoskrnl" whose
// first page begins with 'MZ'.
struct SyntheticKernel {
  FakeMemory mem;
  std::uint64_t system_eprocess = 0xFFFF8000'00010000ull;
  std::uint64_t neighbour_eprocess = 0xFFFF8000'00020000ull;
  std::uint64_t image_virtual = 0xFFFFF800'00000000ull;
  std::uint64_t layout_image_name = 0x2E0;
  std::uint64_t layout_links = 0x448;
  std::uint64_t layout_pid = 0x440;
  std::uint64_t layout_dtb = 0x028;
  std::uint64_t system_dtb = 0x0001AB000ull;  // page-aligned physical
  std::uint64_t neighbour_dtb = 0x0001AC000ull;

  SyntheticKernel() {
    // "System\0\0" at ImageFileName.
    const char name[8] = {'S', 'y', 's', 't', 'e', 'm', 0, 0};
    mem.write(system_eprocess + layout_image_name, name, 8);
    const char other[8] = {'e', 'x', 'p', 'l', 'o', 'r', 'e', 0};
    mem.write(neighbour_eprocess + layout_image_name, other, 8);

    // ActiveProcessLinks: head = &System.links; System.flink = &neighbour.links;
    // neighbour.blink = &System.links (cycle). Blink of System points at the
    // list head beyond scope; give it a self-consistent value.
    const std::uint64_t sys_links = system_eprocess + layout_links;
    const std::uint64_t nbr_links = neighbour_eprocess + layout_links;
    const std::uint64_t flink_sys = nbr_links;
    const std::uint64_t blink_sys = nbr_links + 16;  // some other list node
    const std::uint64_t entry_sys[2] = {flink_sys, blink_sys};
    mem.write(sys_links, entry_sys, sizeof(entry_sys));
    const std::uint64_t entry_nbr[2] = {sys_links, sys_links};
    mem.write(nbr_links, entry_nbr, sizeof(entry_nbr));

    // PIDs: System = 4, neighbour = 4242.
    const std::uint32_t pid_sys = 4;
    const std::uint32_t pid_nbr = 4242;
    mem.write(system_eprocess + layout_pid, &pid_sys, 4);
    mem.write(neighbour_eprocess + layout_pid, &pid_nbr, 4);

    // DTBs.
    mem.write(system_eprocess + layout_dtb, &system_dtb, 8);
    mem.write(neighbour_eprocess + layout_dtb, &neighbour_dtb, 8);

    build_page_tables();
    build_image();
  }

  void build_page_tables() {
    // Map image_virtual -> physical 0x10000000 with one 2 MiB large page at
    // PML4[511] -> PDPT[511] -> PD[entry] (large). Index math for the fixed
    // virtual address below.
    const std::uint64_t pml4_phys = 0x0001A0000ull;
    const std::uint64_t pdpt_phys = 0x0001A1000ull;
    const std::uint64_t pd_phys = 0x0001A2000ull;
    const std::uint64_t present_large =
        real::kernel::derive::kPtePresent | real::kernel::derive::kPteLarge;

    const std::uint64_t pml4_index = (image_virtual >> 39) & 0x1FF;
    const std::uint64_t pdpt_index = (image_virtual >> 30) & 0x1FF;
    const std::uint64_t pd_index = (image_virtual >> 21) & 0x1FF;

    const std::uint64_t pml4_entry = pdpt_phys | real::kernel::derive::kPtePresent;
    const std::uint64_t pdpt_entry = pd_phys | real::kernel::derive::kPtePresent;
    const std::uint64_t pd_entry =
        (0x10000000ull & real::kernel::derive::kPteFrameMask & ~0x1FFFFFull) |
        present_large;
    write_pte(pml4_phys, pml4_index, pml4_entry);
    write_pte(pdpt_phys, pdpt_index, pdpt_entry);
    write_pte(pd_phys, pd_index, pd_entry);

    // System DTB maps the *kernel* address space through the shared PML4
    // entries (kernel PML4 entries are replicated per-process); simplest
    // model: both DTBs point at page tables whose PML4 shares the entry.
    // Use one shared lower structure: point each process DTB's PML4 slot at
    // the same PDPT by making the process PML4 a copy of a shared table.
    for (std::uint64_t dtb: {system_dtb, neighbour_dtb}) {
      // Copy the single relevant PML4 entry into each process PML4.
      write_pte(dtb & real::kernel::derive::kPteFrameMask, pml4_index, pml4_entry);
    }
  }

  void write_pte(std::uint64_t table_phys, std::uint64_t index,
                 std::uint64_t entry) {
    mem.write(table_phys + index * 8, &entry, 8);
  }

  void build_image() {
    const unsigned char mz[2] = {'M', 'Z'};
    mem.write(0x10000000ull, mz, 2);
  }
};

}  // namespace

static void test_eprocess_derivation_full() {
  SyntheticKernel kernel;
  using namespace real::kernel::derive;
  const auto read = [&kernel](std::uint64_t address, void* dst, std::size_t size) {
    return kernel.mem.read(address, dst, size);
  };
  const Derivation result =
      derive_layout(read, kernel.system_eprocess, kernel.image_virtual);
  CHECK(result.ok);
  if (!result.ok) return;
  CHECK_EQ(result.layout.image_file_name, kernel.layout_image_name);
  CHECK_EQ(result.layout.active_process_links, kernel.layout_links);
  CHECK_EQ(result.layout.unique_process_id, kernel.layout_pid);
  CHECK_EQ(result.layout.directory_table_base, kernel.layout_dtb);
  CHECK(result.probes.name_probes > 0);
  CHECK(result.probes.link_cycle_probes > 0);
  CHECK(result.probes.pid_probes > 0);
  CHECK(result.probes.dtb_probes > 0);

  // The derived layout walks the process list with no hardcoded offsets.
  std::vector<std::uint32_t> pids;
  const int visited = walk_process_list(
      read, kernel.system_eprocess, result.layout, DeriveOptions{},
      [&pids](std::uint32_t pid, std::uint64_t) {
        pids.push_back(pid);
        return true;
      });
  CHECK(visited >= 2);
  bool found_system = false;
  bool found_neighbour = false;
  for (std::uint32_t pid: pids) {
    found_system = found_system || pid == 4;
    found_neighbour = found_neighbour || pid == 4242;
  }
  CHECK(found_system);
  CHECK(found_neighbour);
}

static void test_eprocess_derivation_rejects_corrupt_image() {
  SyntheticKernel kernel;
  // Corrupt the name probe: no "System" anywhere in the scan window.
  const char zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  kernel.mem.write(kernel.system_eprocess + 0x2E0, zeros, 8);
  using namespace real::kernel::derive;
  const auto read = [&kernel](std::uint64_t address, void* dst, std::size_t size) {
    return kernel.mem.read(address, dst, size);
  };
  const Derivation result =
      derive_layout(read, kernel.system_eprocess, kernel.image_virtual);
  CHECK(!result.ok);
  CHECK_EQ(result.layout.image_file_name, 0u);
}

static void test_eprocess_walk_guard_bounds_corrupt_list() {
  SyntheticKernel kernel;
  // Corrupt the neighbour's list entry into a self-loop: the walk from the
  // System head must terminate via the guard, never spin forever.
  const std::uint64_t nbr_links = kernel.neighbour_eprocess + kernel.layout_links;
  const std::uint64_t self_loop[2] = {nbr_links, kernel.system_eprocess + kernel.layout_links};
  kernel.mem.write(nbr_links, self_loop, sizeof(self_loop));
  using namespace real::kernel::derive;
  const auto read = [&kernel](std::uint64_t address, void* dst, std::size_t size) {
    return kernel.mem.read(address, dst, size);
  };
  DeriveOptions options;  // default guard: 1024
  const int visited = walk_process_list(
      read, kernel.system_eprocess,
      EprocessLayout{kernel.layout_image_name, kernel.layout_links,
                     kernel.layout_pid, kernel.layout_dtb},
      options,
      [](std::uint32_t, std::uint64_t) { return true; });
  CHECK_EQ(visited, static_cast<int>(options.list_walk_guard));
}

static void test_schema_field_table_walk() {
  using namespace real::cs2::schema;
  const SchemaLayout layout;
  const std::uint64_t class_info = 0xAAAA0000ull;
  const std::uint64_t table = class_info + layout.class_field_table_offset;
  const std::uint64_t name_team = 0xBBBB0000ull;
  const std::uint64_t name_health = 0xBBBB0100ull;
  const std::uint64_t name_other = 0xBBBB0200ull;

  FakeMemory mem;
  const auto put_string = [&mem](std::uint64_t at, const char* text) {
    mem.write(at, text, std::strlen(text) + 1);
  };
  put_string(name_team, "m_iTeamNum");
  put_string(name_health, "m_iHealth");
  put_string(name_other, "m_flThrowVelocity");

  // Three field entries of field_stride bytes: name pointer at +0x08,
  // offset at +0x10 (per SchemaLayout).
  const auto put_entry = [&](std::uint32_t index, std::uint64_t name_pointer,
                             std::uint32_t offset) {
    const std::uint64_t entry =
        table + static_cast<std::uint64_t>(index) * layout.field_stride;
    mem.write(entry + layout.field_name_offset, &name_pointer, 8);
    mem.write(entry + layout.field_offset_offset, &offset, 4);
  };
  put_entry(0, name_team, 0x3E7);
  put_entry(1, name_health, 0x34C);
  put_entry(2, name_other, 0x999);

  const auto read = [&mem](std::uint64_t address, void* dst, std::size_t size) {
    return mem.read(address, dst, size);
  };

  const auto probe = plan_field_probe(layout, class_info, 3, "m_iHealth");
  CHECK(probe.has_value());
  const auto offset = walk_field_table(read, *probe, layout);
  CHECK(offset.has_value());
  CHECK_EQ(offset.value(), 0x34Cull);

  // Absent field: bounded walk, clean miss.
  const auto probe_miss = plan_field_probe(layout, class_info, 3, "m_iAmmo");
  CHECK(!walk_field_table(read, *probe_miss, layout).has_value());

  // Zero field count: never walked.
  const auto probe_empty = plan_field_probe(layout, class_info, 0, "m_iHealth");
  CHECK(!walk_field_table(read, *probe_empty, layout).has_value());

  // Corrupt name pointer inside an entry: skipped, walk continues, clean miss.
  put_entry(1, 0, 0x34C);
  const auto probe_corrupt = plan_field_probe(layout, class_info, 3, "m_iHealth");
  CHECK(!walk_field_table(read, *probe_corrupt, layout).has_value());
}

int main() {
  test_window_bounds_checked_extraction();
  test_required_window_size_and_cap();
  test_chunk_math_and_batch_plan();
  test_pawn_sanity_gates();
  test_offsets_validation_classification();
  test_triple_buffer_torn_free_under_rapid_publish();
  test_entity_cache_diffs_and_generations();
  test_entity_cache_sync_full_walk();
  test_schema_fnv1a_known_vectors();
  test_schema_field_probe_and_memo();
  test_schema_field_table_walk();
  test_frame_pacing_split();
  test_chunked_read_planner();
  test_idle_debouncer();
  test_eprocess_derivation_full();
  test_eprocess_derivation_rejects_corrupt_image();
  test_eprocess_walk_guard_bounds_corrupt_list();

  if (failures != 0) {
    std::printf("radar_entity_math_test: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("radar_entity_math_test: all passed\n");
  return 0;
}
