// radar_external_ledger_test.cpp: host tests for the second-pass external
// assessment primitives (E48, E49, E54, E56, E63, E65, E67, E68).

#include "real/cs2/dead_reckoning.hpp"
#include "real/cs2/entity_layout.hpp"
#include "real/cs2/handle_sanity.hpp"
#include "real/cs2/json_shape.hpp"
#include "real/cs2/pattern_dsl.hpp"
#include "real/cs2/player_name.hpp"
#include "real/cs2/schema_resolver.hpp"
#include "real/cs2/spot_ttl.hpp"
#include "real/cs2/view_point.hpp"
#include "real/cs2/weapon_table.hpp"
#include "real/cs2/world_collision.hpp"
#include "real/kernel/ioctl_interface.hpp"
#include "real/kernel/preflight.hpp"
#include "real/vmx/page_walk.hpp"
#include "real/vmx/shadow_hooks.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

void test_spot_ttl() {
  real::cs2::spot::SpotTtl ttl(1000);
  check(!ttl.visible(7, false, false, 100), "unknown hides");
  check(!ttl.visible(7, true, false, 100), "never spotted hides");
  ttl.observe(7, true, true, 100);
  check(ttl.visible(7, true, false, 500), "recently spotted survives");
  check(ttl.visible(7, true, false, 1100), "ttl boundary inclusive");
  check(!ttl.visible(7, true, false, 1101), "ttl expires");
  ttl.prune(2000);
  check(ttl.size() == 0, "prune drops expired");

  real::cs2::spot::SpotTtl strict(0);
  strict.observe(9, true, true, 100);
  check(strict.visible(9, true, true, 100), "strict current true");
  check(!strict.visible(9, true, false, 101), "strict no memory");
}

void test_dead_reckoning() {
  real::cs2::reckon::DeadReckoner r;
  ac::Vec3 pos{10, 20, 30};
  ac::Vec3 vel{100, 0, 0};
  r.sample(1, pos, vel, 1000);
  ac::Vec3 out{};
  check(r.position(1, 1000, out) && out.x == 10, "sample position");
  check(r.position(1, 1100, out) && out.x > 19.9f && out.x < 20.1f,
        "bounded extrapolation");
  check(r.position(1, 1500, out) && out.x == 10, "past cap snaps to sample");

  r.sample(2, pos, ac::Vec3{5000, 0, 0}, 1000);
  check(r.position(2, 1100, out) && out.x == 10, "speed cap rejects spikes");

  r.sample(3, pos, ac::Vec3{0, 0, 0}, 1000);
  check(r.position(3, 1100, out) && out.x == 10, "stationary stays put");
  r.erase(1);
  check(!r.position(1, 1100, out), "erase");
}

void test_view_point() {
  ac::Vec3 origin{1, 2, 3};
  check(!real::cs2::view::plausible_offset(ac::Vec3{0, 0, 0}), "zero offset unusable");
  check(real::cs2::view::plausible_offset(ac::Vec3{0, 0, 64}), "standing offset");
  check(!real::cs2::view::plausible_offset(ac::Vec3{100, 0, 64}), "wide offset rejected");
  auto eye = real::cs2::view::eye_from(origin, ac::Vec3{0, 0, 64}, true);
  check(eye.valid && eye.origin.z == 67, "offset applied");
  auto fallback = real::cs2::view::eye_from(origin, ac::Vec3{}, false);
  check(fallback.valid && fallback.origin.z == 67, "fallback height");
  check(!real::cs2::view::plausible_fov(0), "fov zero");
  check(!real::cs2::view::plausible_fov(171), "fov too wide");
  check(real::cs2::view::plausible_fov(90), "fov normal");
}

void test_handle_sanity() {
  check(!real::cs2::handle::handle_usable(0), "zero handle");
  check(!real::cs2::handle::handle_usable(0xFFFFFFFF), "invalid handle");
  check(!real::cs2::handle::handle_usable(0x8000), "index over max");
  check(real::cs2::handle::handle_usable(0x00010001), "indexed handle");
  check(!real::cs2::handle::pointer_usable(0), "zero pointer");
  check(!real::cs2::handle::pointer_usable(0xFFFFFFFFFFFFFFFFull), "invalid pointer");
  check(!real::cs2::handle::pointer_usable(0x100), "tiny pointer");
  check(real::cs2::handle::pointer_usable(0x7FF000001000ull), "user pointer");
}

void test_strict_window() {
  using real::cs2::layout::FieldSpan;
  std::vector<FieldSpan> fields{{0x10, 4}, {0x20, 8}};
  auto size = real::cs2::layout::required_window_size_strict(fields);
  check(size.has_value() && *size == 0x28, "strict window size");
  std::vector<FieldSpan> oversize{{0x10, 4}, {real::cs2::layout::kMaxWindowBytes, 8}};
  check(!real::cs2::layout::required_window_size_strict(oversize).has_value(),
        "strict rejects oversize");
  check(real::cs2::layout::required_window_size(oversize) == 0x14,
        "loose still skips oversize");
}

void test_pattern_dsl() {
  real::cs2::sig::ParsedPattern p;
  check(real::cs2::sig::parse_pattern("48 8B 00 ?? 90", p), "parse literal zero");
  check(p.bytes.size() == 5, "byte count");
  check(p.bytes[2] == 0 && !p.wildcard[2], "literal zero is not wildcard");
  check(p.wildcard[3], "explicit wildcard");

  real::cs2::sig::ParsedPattern rel;
  check(real::cs2::sig::parse_pattern("> E8 ?? ?? ?? ?? 90", rel), "parse rel call");
  check(rel.op == real::cs2::sig::ResolveOp::RelCall && rel.op_offset == 0,
        "rel call operator position");

  real::cs2::sig::ParsedPattern rip;
  check(real::cs2::sig::parse_pattern("48 8B 0D * ?? ?? ?? ?? +10 ~", rip),
        "parse rip relative");
  check(rip.op == real::cs2::sig::ResolveOp::RipRelative && rip.post_offset == 0x10 &&
            rip.deref_final,
        "rip flags");

  real::cs2::sig::ParsedPattern abs;
  check(real::cs2::sig::parse_pattern("^ ?? ?? ?? ?? ?? ?? ?? ?? -8", abs),
        "parse absolute");
  check(abs.op == real::cs2::sig::ResolveOp::AbsolutePtr && abs.post_offset == -8,
        "absolute flags");

  real::cs2::sig::ParsedPattern bad;
  check(!real::cs2::sig::parse_pattern("> *", bad), "double operator rejected");
  check(!real::cs2::sig::parse_pattern("", bad), "empty rejected");
  check(!real::cs2::sig::parse_pattern("ZZ", bad), "bad hex rejected");

  // Matching and resolution.
  std::array<std::uint8_t, 32> buf{};
  buf[0] = 0xE8;
  buf[1] = 0x10;  // rel32 = 0x10
  buf[5] = 0x90;
  real::cs2::sig::ParsedPattern call;
  real::cs2::sig::parse_pattern("> E8 ?? ?? ?? ??", call);
  check(real::cs2::sig::matches_at(buf.data(), buf.size(), 0, call), "matches");
  const auto resolved = real::cs2::sig::resolve_match(
      buf.data(), buf.size(), 0, 0x1000, call);
  check(resolved == 0x1015, "rel call resolves from operand end");

  real::cs2::sig::ParsedPattern direct;
  real::cs2::sig::parse_pattern("90", direct);
  check(real::cs2::sig::resolve_match(buf.data(), buf.size(), 5, 0x1000, direct) ==
            0x1000,
        "direct resolve is the match address");
}

struct FakePhys {
  std::map<std::uint64_t, std::array<std::uint8_t, 0x1000>> pages;
  std::uint64_t next = 0x1000;

  std::uint64_t& qword(std::uint64_t pa, std::size_t index) {
    return (reinterpret_cast<std::uint64_t*>(pages[pa & ~0xFFFull].data()))[index];
  }

  std::uint64_t alloc_zero() {
    const std::uint64_t pa = next;
    next += 0x1000;
    pages[pa] = {};
    return pa;
  }

  bool read(std::uint64_t pa, void* out, std::size_t n) {
    const auto it = pages.find(pa & ~0xFFFull);
    if (it == pages.end()) return false;
    std::memcpy(out, it->second.data(), n);
    return true;
  }

  void map_4k(std::uint64_t cr3, std::uint64_t va, std::uint64_t pa,
              bool exec) {
    const auto pml4_pa = cr3;
    const auto pdpt_pa = qword(pml4_pa, 0);
    const auto pd_pa = qword(pdpt_pa, 0);
    const auto pt_pa = qword(pd_pa, 0);
    qword(cr3, (va >> 39) & 0x1FF) = pdpt_pa | 1;
    qword(pdpt_pa, (va >> 30) & 0x1FF) = pd_pa | 1;
    qword(pd_pa, (va >> 21) & 0x1FF) = pt_pa | 1;
    qword(pt_pa, (va >> 12) & 0x1FF) = pa | 1 | (exec ? 0: (1ull << 63));
  }
};

void test_page_walk() {
  FakePhys phys;
  const std::uint64_t cr3 = phys.alloc_zero();
  const std::uint64_t pdpt = phys.alloc_zero();
  const std::uint64_t pd = phys.alloc_zero();
  const std::uint64_t pt = phys.alloc_zero();
  phys.qword(cr3, 0) = pdpt;
  phys.qword(pdpt, 0) = pd;
  phys.qword(pd, 0) = pt;

  const std::uint64_t va = 0x400000;
  const std::uint64_t pa = phys.alloc_zero();
  phys.map_4k(cr3, va, pa, true);

  auto read = [&phys](std::uint64_t address, void* out, std::size_t n) {
    return phys.read(address, out, n);
  };

  real::vmx::pt::PteInfo info{};
  check(real::vmx::pt::query(read, cr3, va, info), "pte resolves");
  check(info.present && info.executable && info.gpa == pa, "pte flags/gpa");

  // Two adjacent zero executable pages form a run.
  const std::uint64_t va2 = va + 0x1000;
  const std::uint64_t pa2 = phys.alloc_zero();
  phys.map_4k(cr3, va2, pa2, true);
  check(real::vmx::pt::find_zero_run(read, cr3, va, va2 + 0x1000, 2) == va,
        "zero run found");

  // A non-zero page breaks the run.
  phys.pages[pa2][0] = 0x90;
  check(real::vmx::pt::find_zero_run(read, cr3, va, va2 + 0x1000, 2) == 0,
        "non-zero breaks run");

  check(!real::vmx::pt::query(read, 0x1234, va, info), "bad cr3");
}

void test_json_shape() {
  const std::string good =
      "{\"fields\":{\"m_iHealth\":844},\"globals\":{\"dwEntityList\":1},"
      "\"top\":2}";
  check(real::cs2::json::is_object(good), "object accepted");
  check(!real::cs2::json::is_object("  [1]"), "array rejected");
  check(!real::cs2::json::is_object(""), "empty rejected");

  auto health = real::cs2::json::key_locations(good, "m_iHealth");
  check(health.size() == 1 && real::cs2::json::key_shape_ok(health[0]),
        "field key under fields");
  auto global = real::cs2::json::key_locations(good, "dwEntityList");
  check(global.size() == 1 && real::cs2::json::key_shape_ok(global[0]),
        "global key under globals");
  auto top = real::cs2::json::key_locations(good, "top");
  check(top.size() == 1 && real::cs2::json::key_shape_ok(top[0]),
        "top-level key");

  const std::string nested =
      "{\"other\":{\"fields\":{\"m_iHealth\":844}},"
      "\"deep\":{\"a\":{\"dwEntityList\":1}}}";
  auto wrong = real::cs2::json::key_locations(nested, "dwEntityList");
  check(wrong.size() == 1 && !real::cs2::json::key_shape_ok(wrong[0]),
        "deep key rejected");

  const std::string escaped = "{\"fields\":{\"m_iHealth\":844},"
                              "\"note\":\"\\\"m_iHealth\\\": 1\"}";
  auto escaped_hits = real::cs2::json::key_locations(escaped, "m_iHealth");
  check(escaped_hits.size() == 1, "escaped quotes do not count as keys");

  const std::string value_text = "{\"fields\":{\"m_iHealth\": 844}}";
  auto value_hit = real::cs2::json::key_locations(value_text, "m_iHealth");
  std::uint64_t value = 0;
  check(value_hit.size() == 1 &&
            real::cs2::json::number_after(value_text, value_hit[0],
                                          std::strlen("m_iHealth"), value) &&
            value == 844,
        "number after key");
  const std::string spaced = "{\"fields\":{\"m_iHealth\" : 844}}";
  auto spaced_hit = real::cs2::json::key_locations(spaced, "m_iHealth");
  check(spaced_hit.size() == 1 &&
            real::cs2::json::number_after(spaced, spaced_hit[0],
                                          std::strlen("m_iHealth"), value) &&
            value == 844,
        "space before colon tolerated");
  const std::string quoted = "{\"fields\":{\"m_iHealth\":\"844\"}}";
  auto quoted_hit = real::cs2::json::key_locations(quoted, "m_iHealth");
  check(quoted_hit.size() == 1 &&
            !real::cs2::json::number_after(quoted, quoted_hit[0],
                                           std::strlen("m_iHealth"), value),
        "quoted number rejected");
}

void test_player_name() {
  check(real::cs2::name::sanitize("  Alice  ") == "Alice", "trim");
  check(real::cs2::name::sanitize("A\tB\nC") == "A B C", "control collapse");
  check(real::cs2::name::sanitize("") == "", "empty");
  const char raw[] = {'A', '\x7f', 'B', '\0'};
  check(real::cs2::name::sanitize(raw, 3) == "A B", "del stripped");
}

void test_weapon_table() {
  check(real::cs2::weapon::display_name(7) == "AK-47", "known weapon");
  check(real::cs2::weapon::display_name(0).empty(), "zero empty");
  check(real::cs2::weapon::display_name(9999) == "Weapon 9999", "unknown");
  check(real::cs2::weapon::is_knife(507), "knife index");
  check(!real::cs2::weapon::is_knife(7), "gun index");
}

void test_schema_resolver() {
  std::map<std::uint64_t, std::vector<std::uint8_t>> memory;
  const auto put = [&memory](std::uint64_t address, const void* data,
                             std::size_t size) {
    auto& block = memory[address];
    block.assign(static_cast<const std::uint8_t*>(data),
                 static_cast<const std::uint8_t*>(data) + size);
  };
  const std::uint64_t scope = 0x10000;
  const std::uint64_t class_info = 0x20000;
  const std::uint64_t class_name = 0x21000;
  const std::uint64_t fields = 0x22000;
  const std::uint64_t field_name = 0x23000;

  const std::uint64_t scope_value = class_info;
  put(scope + 0x40, &scope_value, sizeof(scope_value));
  put(class_name, "C_BaseEntity", 13);
  put(class_info + 0x8, &class_name, sizeof(class_name));
  const std::uint32_t count = 1;
  put(class_info + 0x24, &count, sizeof(count));
  put(class_info + 0x30, &fields, sizeof(fields));
  put(field_name, "m_iHealth", 10);
  put(fields + 0x8, &field_name, sizeof(field_name));
  const std::uint32_t offset = 844;
  put(fields + 0x10, &offset, sizeof(offset));

  const auto read = [&memory](std::uint64_t address, void* out,
                              std::size_t size) -> bool {
    const auto it = memory.find(address);
    if (it == memory.end()) return false;
    const std::size_t available = it->second.size();
    const std::size_t copy = size < available ? size: available;
    std::memcpy(out, it->second.data(), copy);
    if (copy < size) {
      std::memset(static_cast<std::uint8_t*>(out) + copy, 0, size - copy);
    }
    return true;
  };

  const auto found = real::cs2::schema::resolve_field_bruteforce(
      read, scope, "C_BaseEntity", "m_iHealth");
  check(found.found && found.offset == 844, "schema field resolves");

  const auto miss = real::cs2::schema::resolve_field_bruteforce(
      read, scope, "C_BaseEntity", "m_iArmorValue");
  check(!miss.found, "unknown field misses");

  const auto wrong_class = real::cs2::schema::resolve_field_bruteforce(
      read, scope, "C_CSPlayerPawn", "m_iHealth");
  check(!wrong_class.found, "wrong class misses");
}

void test_world_collision() {
  std::map<std::uint64_t, std::vector<std::uint8_t>> memory;
  const auto put = [&memory](std::uint64_t address, const void* data,
                             std::size_t size) {
    auto& block = memory[address];
    block.assign(static_cast<const std::uint8_t*>(data),
                 static_cast<const std::uint8_t*>(data) + size);
  };
  const auto read = [&memory](std::uint64_t address, void* out,
                              std::size_t size) -> bool {
    const auto it = memory.find(address);
    if (it == memory.end()) return false;
    const std::size_t copy = size < it->second.size() ? size: it->second.size();
    std::memcpy(out, it->second.data(), copy);
    if (copy < size) {
      std::memset(static_cast<std::uint8_t*>(out) + copy, 0, size - copy);
    }
    return true;
  };

  const std::uint64_t world = 0x1000;
  const std::uint64_t inner = 0x2000;
  const std::uint64_t bodies = 0x3000;
  const std::uint64_t body = bodies;
  const std::uint64_t mesh_data = 0x5000;
  const std::uint64_t bvh = 0x6000;
  const std::uint64_t verts = 0x7000;
  const std::uint64_t tris = 0x8000;

  put(world + 0x30, &inner, 8);
  put(inner + 0x110, &bodies, 8);
  const std::int32_t count = 1;
  put(bodies + 0x268, &count, 4);
  const std::uint32_t kind = 2;
  put(body + 0x40, &kind, 4);
  put(body + 0xC0, &mesh_data, 8);
  const float scale[3] = {1, 1, 1};
  put(body + 0xB0, scale, 12);
  const float pos[3] = {10, 20, 30};
  put(body + 0x100, pos, 12);
  const float quat[4] = {0, 0, 0, 1};
  put(body + 0x130, quat, 16);

  std::array<std::uint8_t, 0xA0> mdblock{};
  std::memcpy(mdblock.data() + 0x20, &bvh, 8);
  std::memcpy(mdblock.data() + 0x38, &verts, 8);
  std::memcpy(mdblock.data() + 0x50, &tris, 8);
  const std::int32_t node_count = 1;
  std::memcpy(mdblock.data() + 0x28, &node_count, 4);
  put(mesh_data, mdblock.data(), mdblock.size());

  // One leaf node: type 3, payload count 1, range start 0.
  std::array<std::uint8_t, 32> node{};
  const std::uint32_t packed0 = (3u << 30) | 1u;
  const std::uint32_t packed1 = 0;
  std::memcpy(node.data() + 12, &packed0, 4);
  std::memcpy(node.data() + 28, &packed1, 4);
  put(bvh, node.data(), node.size());

  const std::int32_t indices[3] = {0, 1, 2};
  put(tris, indices, sizeof(indices));
  const float vertices[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  put(verts, vertices, sizeof(vertices));

  std::vector<radar_domain::Triangle> out;
  const auto result = real::cs2::collision::extract_world(read, world, out);
  check(result.ok && result.bodies == 1 && out.size() == 1,
        "world mesh extraction");
  check(out[0][0].x == 10 && out[0][0].y == 20 && out[0][0].z == 30,
        "transform applied");

  std::vector<radar_domain::Triangle> empty;
  check(!real::cs2::collision::extract_world(read, 0, empty).ok,
        "missing world fails");
}

void test_page_chunks() {
  const auto plan = real::kernel::plan_chunked_read_pages(0x1FF0, 0x30, 0x20);
  check(plan.size() == 2, "page split produces two chunks");
  check(plan[0].address == 0x1FF0 && plan[0].size == 0x10, "first chunk clamps");
  check(plan[1].address == 0x2000 && plan[1].size == 0x20, "second chunk page aligned");
  check(real::kernel::plan_chunked_read_pages(0, 0, 16).empty(), "zero total");
  check(real::kernel::plan_chunked_read_pages(0xFFFFFFFFFFFFFFF0ull, 0x20, 0x20)
.empty(),
        "overflow refused");
}

void test_preflight() {
  using real::kernel::preflight::SystemStatus;
  using real::kernel::preflight::Verdict;
  SystemStatus s;
  s.is_admin = true;
  s.is_admin_known = true;
  check(real::kernel::preflight::classify(s) == Verdict::Ready,
        "admin with unknowns is ready");

  s.is_admin = false;
  check(real::kernel::preflight::classify(s) == Verdict::Blocked,
        "non-admin blocked");

  s.is_admin = true;
  s.driver_file_known = true;
  s.driver_file = false;
  check(real::kernel::preflight::classify(s) == Verdict::Warn,
        "missing driver warns");

  s.driver_file = true;
  s.secure_boot = true;
  s.secure_boot_known = true;
  s.test_signing = false;
  s.test_signing_known = true;
  check(real::kernel::preflight::classify(s) == Verdict::Warn,
        "secure boot without test signing warns");

  s.test_signing = true;
  check(real::kernel::preflight::classify(s) == Verdict::Ready,
        "hardened but signed is ready");

  check(real::kernel::preflight::describe(s).find("verdict=ready") !=
            std::string::npos,
        "describe carries the verdict");
}

void test_shadow_hooks() {  real::vmx::hook::Table table;
  check(table.add(0x1000, 0x9000, 0x100), "add first");
  check(!table.add(0x1000, 0x9000, 0x100), "duplicate rejected");
  check(table.add(0x1100, 0x9100, 0x101), "add second same page");
  check(table.size() == 2, "size");
  check(table.split_needed(0x1000), "shares large range");
  check(!table.merge_allowed(0x1000), "merge blocked");
  check(table.find(0x1100) != nullptr, "find");
  check(table.remove(0x1100), "remove");
  check(table.merge_allowed(0x1000), "merge allowed after last peer");
  check(table.remove(0x1000), "remove last");
  check(table.size() == 0, "empty");

  real::vmx::hook::Table full;
  for (std::size_t i = 0; i < real::vmx::hook::kEntriesPerPage; ++i) {
    check(full.add(0x2000 + i * 16, 0x100000 + i * 16, 0x200), "fill page");
  }
  check(!full.add(0x2001, 0, 0), "page budget enforced");
}

}  // namespace

int main() {
  test_spot_ttl();
  test_dead_reckoning();
  test_view_point();
  test_handle_sanity();
  test_strict_window();
  test_pattern_dsl();
  test_json_shape();
  test_player_name();
  test_weapon_table();
  test_schema_resolver();
  test_world_collision();
  test_page_chunks();
  test_preflight();
  test_page_walk();
  test_shadow_hooks();
  if (failures != 0) {
    std::printf("%d check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_external_ledger_test: all checks passed\n");
  return 0;
}
