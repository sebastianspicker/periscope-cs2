// pinned_buffer.hpp — RAII page-locked buffer for kernel/hypercall copies
// (E69).
//
// A kernel driver or hypervisor must not touch a pageable user page mid-copy.
// The donor locks the destination before the hypercall and unlocks after.
// This is the RAII form: lock on construction (best effort), unlock on
// destruction, never copyable.
//
// On non-Windows hosts the lock is a no-op so pure code still compiles.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#endif

namespace real::kernel {

class PinnedBuffer {
 public:
  explicit PinnedBuffer(std::size_t size) : bytes_(size) {
#if LR_PLATFORM_WINDOWS
    if (!bytes_.empty()) {
      locked_ = VirtualLock(bytes_.data(), bytes_.size()) != FALSE;
    }
#endif
  }

  ~PinnedBuffer() {
#if LR_PLATFORM_WINDOWS
    if (locked_ && !bytes_.empty()) {
      VirtualUnlock(bytes_.data(), bytes_.size());
    }
#endif
  }

  PinnedBuffer(const PinnedBuffer&) = delete;
  PinnedBuffer& operator=(const PinnedBuffer&) = delete;
  PinnedBuffer(PinnedBuffer&& other) noexcept
      : bytes_(std::move(other.bytes_)), locked_(other.locked_) {
    other.locked_ = false;
  }
  PinnedBuffer& operator=(PinnedBuffer&& other) noexcept {
    if (this != &other) {
      release();
      bytes_ = std::move(other.bytes_);
      locked_ = other.locked_;
      other.locked_ = false;
    }
    return *this;
  }

  std::uint8_t* data() { return bytes_.data(); }
  const std::uint8_t* data() const { return bytes_.data(); }
  std::size_t size() const { return bytes_.size(); }
  bool empty() const { return bytes_.empty(); }
  bool locked() const { return locked_; }
  bool usable() const {
#if LR_PLATFORM_WINDOWS
    return locked_ || bytes_.empty();
#else
    return true;
#endif
  }

 private:
  void release() {
#if LR_PLATFORM_WINDOWS
    if (locked_ && !bytes_.empty()) {
      VirtualUnlock(bytes_.data(), bytes_.size());
    }
#endif
    locked_ = false;
  }

  std::vector<std::uint8_t> bytes_;
  bool locked_ = false;
};

}  // namespace real::kernel
