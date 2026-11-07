// shadow_hooks.hpp: SLAT shadow-hook bookkeeping (E68).
//
// Models the donor's hook-entry list without installing anything: a fixed
// 16-byte entry, at most 256 entries per 4 KiB page, original PTE state kept
// for exact restoration, and split/merge predicates based on whether another
// hook shares the 2 MiB range.
//
// Pure and host-testable.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace real::vmx::hook {

inline constexpr std::size_t kEntriesPerPage = 256;
inline constexpr std::uint64_t kPageSize = 0x1000;
inline constexpr std::uint64_t kLargeRange = 0x200000;

struct Entry {
  std::uint64_t target_gpa = 0;
  std::uint64_t shadow_gpa = 0;
  std::uint64_t original_pfn = 0;
  bool original_read = true;
  bool original_write = true;
  bool original_execute = true;
  bool split = false;  ///< 4 KiB split active for this 2 MiB range
  bool used = false;
};

class Table {
 public:
  /// Add a hook. Fails when the page budget is exhausted or the target exists.
  bool add(std::uint64_t target_gpa, std::uint64_t shadow_gpa,
           std::uint64_t original_pfn) {
    const std::uint64_t page = target_gpa & ~(kPageSize - 1);
    std::size_t used_on_page = 0;
    for (const Entry& e : entries_) {
      if (!e.used) continue;
      if (e.target_gpa == target_gpa) return false;
      if ((e.target_gpa & ~(kPageSize - 1)) == page) ++used_on_page;
    }
    if (used_on_page >= kEntriesPerPage) return false;

    Entry e;
    e.target_gpa = target_gpa;
    e.shadow_gpa = shadow_gpa;
    e.original_pfn = original_pfn;
    e.used = true;
    entries_.push_back(e);
    mark_split(target_gpa, true);
    return true;
  }

  bool remove(std::uint64_t target_gpa) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->used && it->target_gpa == target_gpa) {
        const bool split_needed = shares_large_range(target_gpa, &*it);
        entries_.erase(it);
        if (!split_needed) mark_split(target_gpa, false);
        return true;
      }
    }
    return false;
  }

  const Entry* find(std::uint64_t target_gpa) const {
    for (const Entry& e : entries_) {
      if (e.used && e.target_gpa == target_gpa) return &e;
    }
    return nullptr;
  }

  bool split_needed(std::uint64_t target_gpa) const {
    return shares_large_range(target_gpa, nullptr);
  }

  bool merge_allowed(std::uint64_t target_gpa) const {
    return !shares_large_range(target_gpa, nullptr);
  }

  std::size_t size() const {
    std::size_t n = 0;
    for (const Entry& e : entries_) {
      if (e.used) ++n;
    }
    return n;
  }

  void clear() { entries_.clear(); }

 private:
  bool shares_large_range(std::uint64_t target_gpa,
                          const Entry* exclude) const {
    const std::uint64_t lo = target_gpa & ~(kLargeRange - 1);
    const std::uint64_t hi = lo + kLargeRange;
    for (const Entry& e : entries_) {
      if (!e.used || &e == exclude || e.target_gpa == target_gpa) continue;
      if (e.target_gpa >= lo && e.target_gpa < hi) return true;
    }
    return false;
  }

  void mark_split(std::uint64_t target_gpa, bool split) {
    const std::uint64_t lo = target_gpa & ~(kLargeRange - 1);
    const std::uint64_t hi = lo + kLargeRange;
    for (Entry& e : entries_) {
      if (e.used && e.target_gpa >= lo && e.target_gpa < hi) e.split = split;
    }
  }

  std::vector<Entry> entries_;
};

}  // namespace real::vmx::hook
