#pragma once

// Handle table model — tracks handle creation, visibility, and lifecycle.
// Blue's HandleTableSensor uses this to detect processes holding handles to the game.
// Critical insight: handle table entries are visible regardless of how the handle
// was obtained (syscall/IAT hiding doesn't help).

#include "ac/types.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace sim {

/// A single handle entry in the system handle table.
struct HandleEntry {
  std::uint32_t owner_pid = 0;     // Process that owns this handle
  std::uint32_t target_pid = 0;    // Process that the handle points to
  std::uint32_t access_mask = 0;   // E.g., PROCESS_VM_READ (0x0010)
  ac::HandleAcquisitionModel acquisition = ac::HandleAcquisitionModel::None;
  std::uint64_t created_tick = 0;   // World tick when handle was opened
  std::uint64_t closed_tick = 0;    // World tick when handle was closed (0 = still open)
  bool ephemeral = false;           // true if open → read → close in < 1 tick
  bool hidden_during_enum = false;  // Handle hidden from NtQuerySystemInformation
  bool via_proxy = false;           // Handle obtained through proxy/duplication
};

/// System-wide handle table simulation.
/// Red strategies add entries when they open handles.
/// Blue strategies query entries to detect suspicious access patterns.
class HandleTable {
 public:
  HandleTable() = default;

  /// Add a handle entry to the table.
  void add_handle(const HandleEntry& entry);

  /// Convenience builder used by World::record_handle_table.
  void open(std::uint32_t owner_pid, std::uint32_t target_pid,
            std::uint32_t access_mask, ac::HandleAcquisitionModel acquisition,
            std::uint64_t tick, bool ephemeral = false, bool via_proxy = false,
            bool hidden_during_enum = false);

  /// Mark a handle as closed at |tick| (0 = mark closed without tick).
  void close_handle(std::uint32_t owner_pid, std::uint32_t target_pid,
                    std::uint64_t tick = 0xFFFFFFFF);

  /// Close all handles owned by |owner_pid|.
  void close_all_from(std::uint32_t owner_pid, std::uint64_t tick = 0xFFFFFFFF);

  /// Get all handles pointing to a specific target process (including closed).
  std::vector<HandleEntry> handles_to(std::uint32_t target_pid) const;

  /// Get currently-open handles pointing to |target_pid|.
  std::vector<HandleEntry> open_handles_to(std::uint32_t target_pid,
                                           bool include_hidden = true) const;

  /// Get all handles owned by a specific process.
  std::vector<HandleEntry> handles_from(std::uint32_t owner_pid) const;

  /// Open handles owned by |owner_pid|.
  std::vector<HandleEntry> open_handles_from(std::uint32_t owner_pid) const;

  /// Number of unique processes holding *open* handles to |target_pid|.
  int unique_owners_for(std::uint32_t target_pid) const;

  /// Number of open handles with VmRead-equivalent access to |target_pid|.
  int open_vm_read_count(std::uint32_t target_pid) const;

  /// Count of ephemeral open→close scars targeting |target_pid|.
  int ephemeral_count_for(std::uint32_t target_pid) const;

  /// Total number of handles in the table (open + closed history).
  int total_handles() const { return static_cast<int>(entries_.size()); }

  /// Number of currently open handles.
  int open_handle_count() const;

  /// Clear all entries (e.g., on World reset).
  void clear();

  /// Get all entries (for blue enumeration).
  const std::vector<HandleEntry>& all_entries() const { return entries_; }

 private:
  std::vector<HandleEntry> entries_;
};

}  // namespace sim
