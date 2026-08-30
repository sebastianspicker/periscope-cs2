// memory_backend.hpp — abstract IMemoryBackend read surface shared by red tiers.

#pragma once

#include "ac/types.hpp"

#include <cstdint>
#include <string_view>

namespace ac {

/// Common read surface used by all red tiers (and lab mocks).

class IMemoryBackend {
 public:
  virtual ~IMemoryBackend() = default;

  virtual Tier tier() const = 0;
  virtual std::string_view name() const = 0;

  /// Attach to a lab target id (never a live game name hard-coded here).
  virtual Status attach(std::uint32_t target_id) = 0;
  virtual void detach() = 0;
  virtual bool is_attached() const = 0;

  /// On Status::Ok, bytes must contain exactly req.size bytes. Backends must
  /// reject overflowed or partially materialized ranges rather than returning
  /// a short success result to typed callers.
  [[nodiscard]] virtual ReadResult read(const ReadRequest& req) = 0;
};

}  // namespace ac
