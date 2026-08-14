// lab_memory.hpp — lab fixture process / memory backend for isolated unit tests.
// Provides attachable target_id.

#pragma once

#include "ac/memory_backend.hpp"
#include "cs2/signatures.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace lab {

// Plant real CS2 byte pattern markers into game memory so the AOB scanner
// simulation can find them. This makes the lab demonstrate real cheat
// signature scanning.
void plant_cs2_pattern_markers(std::vector<std::uint8_t>& memory,
                               std::uint64_t base_address,
                               const cs2::SignatureDatabase& db);

// Materialize an IDA-style hex pattern into concrete bytes (wildcards → 0).
std::vector<std::uint8_t> materialize_pattern_bytes(std::string_view hex);

struct LabMemoryStats {
  std::uint64_t read_ops = 0;
  std::uint64_t read_bytes = 0;
  std::uint64_t write_ops = 0;
  std::uint64_t write_bytes = 0;
  std::uint64_t scatter_ops = 0;
  bool sequential_burst = false;
  bool bulk_read = false;
};

struct ScatterReadRequest {
  std::uint64_t address = 0;
  std::size_t size = 0;
};

struct ScatterReadResult {
  ac::Status status = ac::Status::Ok;
  std::vector<ac::ReadResult> parts;
};

/// Red-safe backend: only reads FixtureProcess by id.
class LabMemoryBackend final : public ac::IMemoryBackend {
 public:
  ac::Tier tier() const override { return ac::Tier::T0_UsermodeRpm; }
  std::string_view name() const override { return "lab_fixture"; }

  ac::Status attach(std::uint32_t target_id) override;
  void detach() override;
  bool is_attached() const override { return attached_; }
  ac::ReadResult read(const ac::ReadRequest& req) override;

  // Optional write path for planting scars during tests.
  ac::Status write(std::uint64_t address, std::span<const std::uint8_t> data);

  // Multi-range read used by scattered entity collection lessons.
  ScatterReadResult scatter_read(const std::vector<ScatterReadRequest>& reqs);

  std::uint32_t target_id() const { return target_id_; }
  const LabMemoryStats& stats() const { return stats_; }
  void reset_stats();

 private:
  bool attached_ = false;
  std::uint32_t target_id_ = 0;
  LabMemoryStats stats_{};
  std::uint64_t last_read_addr_ = 0;
  bool have_last_read_ = false;
};

}  // namespace lab
