// fixture_process.cpp — lab fixture process / memory backend for isolated unit tests.
// Provides attachable target_id.

#include "lab/fixture_process.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>

namespace lab {
namespace {

std::mutex g_registry_mu;
std::map<std::uint32_t, FixtureProcess*> g_registry;
constexpr std::size_t kMaxFixtureImageBytes = 128u * 1024u * 1024u;

bool range_fits(std::uint64_t address, std::uint64_t base,
                std::size_t size, std::size_t extent,
                std::size_t* offset) {
  if (address < base) return false;
  const auto delta = address - base;
  if (delta > std::numeric_limits<std::size_t>::max()) return false;
  const auto start = static_cast<std::size_t>(delta);
  if (size > std::numeric_limits<std::size_t>::max() - start) return false;
  if (start > extent || size > extent - start) return false;
  *offset = start;
  return true;
}

}  // namespace

// FixtureProcess::FixtureProcess: In-memory lab process fixture with base address + bytes.
FixtureProcess::FixtureProcess(std::uint32_t id) : id_(id) {
  image_.assign(0x2000, 0);
  plant_synthetic_entities();
  plant_identity_view_matrix();
  plant_default_modules();
}

void FixtureProcess::write_bytes(std::uint64_t address,
                                 std::span<const std::uint8_t> data) {
  if (address < base_) return;
  const auto delta = address - base_;
  if (delta > std::numeric_limits<std::size_t>::max()) return;
  const auto off = static_cast<std::size_t>(delta);
  if (data.size() > std::numeric_limits<std::size_t>::max() - off) return;
  const auto end = off + data.size();
  if (end > kMaxFixtureImageBytes) return;
  if (end > image_.size()) {
    image_.resize(end);
  }
  std::copy(data.begin(), data.end(), image_.begin() +
                                      static_cast<std::ptrdiff_t>(off));
}

ac::ReadResult FixtureProcess::read_bytes(std::uint64_t address,
                                          std::size_t size) const {
  ac::ReadResult out;
  std::size_t off = 0;
  if (!range_fits(address, base_, size, image_.size(), &off)) {
    out.status = ac::Status::InvalidArgument;
    return out;
  }
  out.bytes.assign(image_.begin() + static_cast<std::ptrdiff_t>(off),
                   image_.begin() + static_cast<std::ptrdiff_t>(off + size));
  out.status = ac::Status::Ok;
  return out;
}

void FixtureProcess::plant_synthetic_entities() {
  // Layout:
  // base+0x00: u32 count
  // base+0x10: entity0 { f32 x,y,z, u8 team, u8 alive, pad }
  std::vector<FixtureEntity> ents = {
      {10.f, 0.f, 20.f, 2, 1, {0, 0}},
      {30.f, 0.f, 40.f, 3, 1, {0, 0}},
      {50.f, 0.f, 10.f, 2, 0, {0, 0}},  // dead teammate residual
  };
  plant_entities(ents);
}

void FixtureProcess::plant_entities(const std::vector<FixtureEntity>& entities) {
  const auto count = static_cast<std::uint32_t>(
      std::min<std::size_t>(entities.size(),
                            std::numeric_limits<std::uint32_t>::max()));
  write_bytes(base_ + 0x00,
              {reinterpret_cast<const std::uint8_t*>(&count),
               reinterpret_cast<const std::uint8_t*>(&count) + sizeof(count)});

  // Clear a generous span then write entities.
  if (entities.size() >
      std::numeric_limits<std::size_t>::max() / sizeof(FixtureEntity)) {
    return;
  }
  const std::size_t table_bytes = entities.size() * sizeof(FixtureEntity);
  if (table_bytes > 0) {
    std::vector<std::uint8_t> zeros(table_bytes, 0);
    write_bytes(base_ + 0x10, zeros);
    write_bytes(base_ + 0x10,
                {reinterpret_cast<const std::uint8_t*>(entities.data()),
                 reinterpret_cast<const std::uint8_t*>(entities.data()) +
                     table_bytes});
  }
}

void FixtureProcess::plant_view_matrix(const float matrix[16]) {
  write_bytes(base_ + kViewMatrixRel,
              {reinterpret_cast<const std::uint8_t*>(matrix),
               reinterpret_cast<const std::uint8_t*>(matrix) + sizeof(float) * 16});
}

void FixtureProcess::plant_identity_view_matrix() {
  float m[16] = {};
  m[0] = m[5] = m[10] = m[15] = 1.0f;
  plant_view_matrix(m);
}

void FixtureProcess::plant_modules(const std::vector<FixtureModule>& modules) {
  modules_ = modules;
  std::uint32_t count = static_cast<std::uint32_t>(modules.size());
  write_bytes(base_ + kModuleDirRel,
              {reinterpret_cast<const std::uint8_t*>(&count),
               reinterpret_cast<const std::uint8_t*>(&count) + sizeof(count)});
  std::uint64_t cursor = base_ + kModuleDirRel + 0x10;
  for (const auto& mod : modules) {
    // [u64 base][u32 size][u32 name_len][name bytes]
    write_bytes(cursor, {reinterpret_cast<const std::uint8_t*>(&mod.base),
                         reinterpret_cast<const std::uint8_t*>(&mod.base) +
                             sizeof(mod.base)});
    cursor += sizeof(mod.base);
    const std::uint32_t size = static_cast<std::uint32_t>(mod.size);
    write_bytes(cursor, {reinterpret_cast<const std::uint8_t*>(&size),
                         reinterpret_cast<const std::uint8_t*>(&size) +
                             sizeof(size)});
    cursor += sizeof(size);
    const std::uint32_t name_len = static_cast<std::uint32_t>(mod.name.size());
    write_bytes(cursor,
                {reinterpret_cast<const std::uint8_t*>(&name_len),
                 reinterpret_cast<const std::uint8_t*>(&name_len) +
                     sizeof(name_len)});
    cursor += sizeof(name_len);
    if (!mod.name.empty()) {
      write_bytes(cursor, {reinterpret_cast<const std::uint8_t*>(mod.name.data()),
                           reinterpret_cast<const std::uint8_t*>(mod.name.data()) +
                               mod.name.size()});
      cursor += mod.name.size();
    }
    cursor = (cursor + 7u) & ~std::uint64_t{7u};  // align
  }
}

void FixtureProcess::plant_default_modules() {
  plant_modules({
      {"client.dll", base_ + 0x200000, 0x1A00000},
      {"engine2.dll", base_ + 0x4000000, 0x800000},
      {"schemasystem.dll", base_ + 0x5000000, 0x200000},
  });
}

std::uint32_t FixtureProcess::entity_count() const {
  auto rr = read_bytes(base_ + 0x00, sizeof(std::uint32_t));
  if (rr.status != ac::Status::Ok || rr.bytes.size() < sizeof(std::uint32_t)) {
    return 0;
  }
  std::uint32_t count = 0;
  std::array<std::uint8_t, sizeof(count)> bytes{};
  std::copy_n(rr.bytes.begin(), bytes.size(), bytes.begin());
  return std::bit_cast<std::uint32_t>(bytes);
}

std::vector<FixtureEntity> FixtureProcess::read_entities() const {
  const auto count = entity_count();
  std::vector<FixtureEntity> out;
  if (count == 0 || count > 256) return out;
  static_assert(256 <=
                std::numeric_limits<std::size_t>::max() / sizeof(FixtureEntity));
  const auto byte_count = static_cast<std::size_t>(count) * sizeof(FixtureEntity);
  auto rr = read_bytes(base_ + 0x10, byte_count);
  if (rr.status != ac::Status::Ok ||
      rr.bytes.size() != byte_count) {
    return out;
  }
  out.resize(count);
  std::copy(rr.bytes.begin(), rr.bytes.end(),
            reinterpret_cast<std::uint8_t*>(out.data()));
  return out;
}

void FixtureProcess::resize(std::size_t bytes) { image_.resize(bytes, 0); }

void FixtureProcess::clear_image() {
  std::fill(image_.begin(), image_.end(), 0);
  modules_.clear();
}

FixtureProcess& global_fixture() {
  static FixtureProcess fx;
  // Always re-bind after clear_fixture_registry() so unit tests stay isolated.
  register_fixture(&fx);
  return fx;
}

bool register_fixture(FixtureProcess* fx) {
  if (!fx) return false;
  std::lock_guard lock(g_registry_mu);
  g_registry[fx->id()] = fx;
  return true;
}

void clear_fixture_registry() {
  std::lock_guard lock(g_registry_mu);
  g_registry.clear();
}

FixtureProcess& fixture_by_id(std::uint32_t id) {
  {
    std::lock_guard lock(g_registry_mu);
    const auto it = g_registry.find(id);
    if (it != g_registry.end() && it->second != nullptr) return *it->second;
  }
  return global_fixture();
}

}  // namespace lab
