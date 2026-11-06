// fixture_process.hpp — lab fixture process / memory backend for isolated unit tests.
// Provides attachable target_id.

#pragma once

#include "ac/types.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lab {

struct FixtureModule {
  std::string name;
  std::uint64_t base = 0;
  std::size_t size = 0;
};

struct FixtureEntity {
  float x = 0, y = 0, z = 0;
  std::uint8_t team = 0;
  std::uint8_t alive = 0;
  std::uint8_t pad[2]{};
};

/// In-process fake "game memory" for red/blue unit tests.
/// Does not open real external processes unless OS APIs are explicitly enabled.
///
/// Layout (shared with EntityPipeline / plant_lab_entities):
///   base+0x00: u32 count
///   base+0x10: FixtureEntity[count]  (16 bytes each)
/// Optional extensions:
///   base+0x1000: f32 view_matrix[16]
///   base+0x1100: module directory (count + entries)
class FixtureProcess {
 public:
  static constexpr std::uint32_t kDefaultId = 4242;
  static constexpr std::uint64_t kDefaultBase = 0x10000000ull;
  static constexpr std::uint64_t kViewMatrixRel = 0x1000;
  static constexpr std::uint64_t kModuleDirRel = 0x1100;

  explicit FixtureProcess(std::uint32_t id = kDefaultId);

  std::uint32_t id() const { return id_; }
  std::uint64_t base_address() const { return base_; }
  std::size_t image_size() const { return image_.size(); }
  const std::vector<std::uint8_t>& image() const { return image_; }
  std::vector<std::uint8_t>& image_mut() { return image_; }

  void write_bytes(std::uint64_t address, std::span<const std::uint8_t> data);
  ac::ReadResult read_bytes(std::uint64_t address, std::size_t size) const;

  /// Plant a tiny synthetic entity blob at a known VA for pipeline tests.
  void plant_synthetic_entities();
  void plant_entities(const std::vector<FixtureEntity>& entities);

  /// Identity view-projection at base+0x1000.
  void plant_view_matrix(const float matrix[16]);
  void plant_identity_view_matrix();

  /// Module directory for PEB-walk lessons.
  void plant_modules(const std::vector<FixtureModule>& modules);
  void plant_default_modules();

  std::uint32_t entity_count() const;
  std::vector<FixtureEntity> read_entities() const;
  std::vector<FixtureModule> modules() const { return modules_; }

  void resize(std::size_t bytes);
  void clear_image();

 private:
  std::uint32_t id_;
  std::uint64_t base_ = kDefaultBase;
  std::vector<std::uint8_t> image_;
  std::vector<FixtureModule> modules_;
};

// global_fixture: free function for this educational unit.
FixtureProcess& global_fixture();

// Optional secondary fixtures keyed by id (for multi-target lessons).
FixtureProcess& fixture_by_id(std::uint32_t id);
bool register_fixture(FixtureProcess* fx);
void clear_fixture_registry();

}  // namespace lab
