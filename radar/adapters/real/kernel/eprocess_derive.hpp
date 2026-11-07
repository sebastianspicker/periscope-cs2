// eprocess_derive.hpp: Runtime derivation of _EPROCESS field offsets from
// self-validating invariants (assessment ledger E16).
//
// Hardcoded EPROCESS offsets (UniqueProcessId 0x440, ActiveProcessLinks
// 0x448, ...) rot with every Windows build. The derivation strategy instead
// treats the layout as unknown and *derives* it from invariants that any
// correct layout must satisfy, each candidate offset is accepted only when
// the whole structure stays self-consistent:
//
//   ImageFileName       scanning the System EPROCESS for "System\0" at an
//                       aligned offset; the name is constant.
//   ActiveProcessLinks  the candidate LIST_ENTRY must close a cycle:
//                       Flink->Blink == &entry (validated through reads).
//   UniqueProcessId     the System process has PID 4; the candidate offset
//                       holds exactly 4 in the System EPROCESS while the
//                       *neighbouring* entry in the list holds something
//                       different (uniqueness guard).
//   DirectoryTableBase  a page-aligned value below the physical-memory cap
//                       whose page tables actually translate a known kernel
//                       image address to an 'MZ' page.
//
// Every predicate doubles as a blue-side observable in the strategy-pair
// model (E20): each probe is an extra read transaction, each validation a
// false-positive probability. The derivation reports which predicates fired
// so the cost is visible, not hidden.
//
// The derivation is parameterized by a read functor
// ``bool(std::uint64_t address, void* dst, std::size_t size)``, on Windows
// this reads kernel/physical memory through the tier backend; in the host
// test it reads a synthetic memory image. Nothing here touches a platform
// header.
//
// Scope note: PEB derivation (Ldr-cycle invariant) is deliberately not
// implemented here; the donor-worker path obtains the PEB from usermode,
// and the kernel half only needs PID/links/DTB to walk process lists and
// translate addresses.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace real::kernel::derive {

/// Derived layout: field offsets inside _EPROCESS.
struct EprocessLayout {
  std::uint64_t image_file_name = 0;
  std::uint64_t active_process_links = 0;
  std::uint64_t unique_process_id = 0;
  std::uint64_t directory_table_base = 0;
};

/// Which predicates fired while deriving (cost model for the pair model).
struct DerivationProbes {
  int name_probes = 0;
  int link_cycle_probes = 0;
  int pid_probes = 0;
  int dtb_probes = 0;
};

/// Scan/validation knobs. Bounds keep every scan finite even on corrupt
/// memory; they are *our* named constants (the anti-pattern E44 warns about
/// is an unnamed magic bound), documented and overridable.
struct DeriveOptions {
  std::size_t max_layout_scan_bytes = 0x800;  // EPROCESS header region scan
  std::uint64_t max_physical_address = 0x40'0000'0000ull;  // 256 GiB cap
  std::uint64_t system_process_id = 4;
  std::size_t list_walk_guard = 1024;  // corrupt-list iteration cap
};

/// The result: a layout only when every invariant held.
struct Derivation {
  EprocessLayout layout{};
  DerivationProbes probes{};
  bool ok = false;
};

// ── Page-walk primitive (4-level x64, large-page aware) ────────────

inline constexpr std::uint64_t kPtePresent = 0x1;
inline constexpr std::uint64_t kPteLarge = 0x80;
inline constexpr std::uint64_t kPteFrameMask = 0x000FFFFFFFFFF000ull;
inline constexpr std::uint64_t kPhysPageMask = 0xFFFull;

/// Translate one virtual address through ``dtb``; returns the physical
/// address or nullopt when any level is not present. Supports 1 GiB / 2 MiB
/// / 4 KiB pages. ``read`` fetches page-table entries (physical addresses).
template <typename ReadFn>
std::optional<std::uint64_t> translate(ReadFn read, std::uint64_t dtb,
                                       std::uint64_t virtual_address) {
  const std::size_t levels = 4;
  const std::uint64_t shift[levels] = {39, 30, 21, 12};
  std::uint64_t table = dtb & kPteFrameMask;
  std::uint64_t phys = 0;
  for (std::size_t level = 0; level < levels; ++level) {
    const std::uint64_t entry_address =
        table + ((virtual_address >> shift[level]) & 0x1FF) * 8;
    std::uint64_t entry = 0;
    if (!read(entry_address, &entry, sizeof(entry))) return std::nullopt;
    if (!(entry & kPtePresent)) return std::nullopt;
    if (level < levels - 1 && (entry & kPteLarge)) {
      // Large page: remaining low bits are the page offset.
      const std::uint64_t page_mask = (1ull << shift[level]) - 1;
      phys = (entry & kPteFrameMask & ~page_mask) | (virtual_address & page_mask);
      return phys;
    }
    table = entry & kPteFrameMask;
    if (level == levels - 1) {
      phys = (entry & kPteFrameMask) | (virtual_address & kPhysPageMask);
    }
  }
  return phys;
}

// ── Predicates ──────────────────────────────────────────────────────

/// Read little-endian scalar at ``address`` through ``read``.
template <typename T, typename ReadFn>
bool read_scalar(ReadFn read, std::uint64_t address, T& out) {
  return read(address, &out, sizeof(T));
}

/// Predicate: candidate offset holds "System\0" (the constant image name of
/// the initial process object).
template <typename ReadFn>
bool probe_image_file_name(ReadFn read, std::uint64_t eprocess,
                           std::uint64_t candidate_offset,
                           DerivationProbes& probes) {
  ++probes.name_probes;
  char name[8] = {};
  if (!read(eprocess + candidate_offset, name, sizeof(name))) return false;
  return std::memcmp(name, "System\0", 7) == 0 && name[7] == '\0';
}

/// Predicate: candidate LIST_ENTRY closes a cycle, Flink->Blink points back
/// at the entry itself. Requires two bounded dereferences per candidate.
template <typename ReadFn>
bool probe_active_process_links(ReadFn read, std::uint64_t eprocess,
                                std::uint64_t candidate_offset,
                                DerivationProbes& probes) {
  ++probes.link_cycle_probes;
  const std::uint64_t entry = eprocess + candidate_offset;
  std::uint64_t flink = 0;
  if (!read_scalar<std::uint64_t>(read, entry, flink) || flink == 0) return false;
  std::uint64_t flink_blink = 0;
  if (!read_scalar<std::uint64_t>(read, flink + sizeof(std::uint64_t), flink_blink)) {
    return false;
  }
  return flink_blink == entry;
}

/// Predicate: candidate offset holds the System PID (4) in the System
/// EPROCESS while the next list entry holds a different value.
template <typename ReadFn>
bool probe_unique_process_id(ReadFn read, std::uint64_t eprocess,
                             std::uint64_t links_offset,
                             std::uint64_t candidate_offset,
                             const DeriveOptions& options,
                             DerivationProbes& probes) {
  ++probes.pid_probes;
  std::uint32_t system_pid = 0;
  if (!read_scalar<std::uint32_t>(read, eprocess + candidate_offset, system_pid)) {
    return false;
  }
  if (system_pid != options.system_process_id) return false;
  // Uniqueness: the neighbour in the active-process list must differ.
  std::uint64_t flink = 0;
  if (!read_scalar<std::uint64_t>(read, eprocess + links_offset, flink) || flink == 0) {
    return false;
  }
  std::uint32_t neighbour_pid = 0;
  if (!read_scalar<std::uint32_t>(read, flink - links_offset + candidate_offset,
                                  neighbour_pid)) {
    return false;
  }
  return neighbour_pid != options.system_process_id;
}

/// Predicate: candidate DTB is within the physical cap and its page tables
/// translate ``image_virtual_base`` to a page whose first bytes are 'MZ'
/// (a mapped PE image). Low CR3 control bits (PCID/PCD/PWT) are tolerated -
/// the translation masks them; the MZ probe carries the real weight.
template <typename ReadFn>
bool probe_directory_table_base(ReadFn read, std::uint64_t candidate_dtb,
                                std::uint64_t image_virtual_base,
                                const DeriveOptions& options,
                                DerivationProbes& probes) {
  ++probes.dtb_probes;
  if (candidate_dtb == 0 || candidate_dtb > options.max_physical_address) return false;
  const auto phys = translate(read, candidate_dtb, image_virtual_base);
  if (!phys) return false;
  unsigned char magic[2] = {};
  if (!read(*phys, magic, sizeof(magic))) return false;
  return magic[0] == 'M' && magic[1] == 'Z';
}

// ── The derivation driver ───────────────────────────────────────────

/// Derive the layout from the System EPROCESS. ``image_virtual_base`` is a
/// known-mapped kernel image address (e.g. the ntoskrnl base) used by the
/// DTB predicate. Scans are bounded by ``DeriveOptions``.
template <typename ReadFn>
Derivation derive_layout(ReadFn read,
                         std::uint64_t system_eprocess,
                         std::uint64_t image_virtual_base,
                         const DeriveOptions& options = {}) {
  Derivation result;
  if (system_eprocess == 0) return result;

  // 1. ImageFileName: first aligned offset holding "System\0".
  bool have_name = false;
  for (std::size_t offset = 0; offset + 8 <= options.max_layout_scan_bytes;
       offset += 8) {
    if (probe_image_file_name(read, system_eprocess, offset, result.probes)) {
      result.layout.image_file_name = offset;
      have_name = true;
      break;
    }
  }
  if (!have_name) return result;

  // 2. ActiveProcessLinks: first offset whose LIST_ENTRY closes a cycle.
  bool have_links = false;
  for (std::size_t offset = 0; offset + 16 <= options.max_layout_scan_bytes;
       offset += 8) {
    if (offset == result.layout.image_file_name) continue;
    if (probe_active_process_links(read, system_eprocess, offset,
                                   result.probes)) {
      result.layout.active_process_links = offset;
      have_links = true;
      break;
    }
  }
  if (!have_links) return result;

  // 3. UniqueProcessId: holds 4 in System, differs in the neighbour.
  bool have_pid = false;
  for (std::size_t offset = 0; offset + 4 <= options.max_layout_scan_bytes;
       offset += 4) {
    if (offset == result.layout.active_process_links) continue;
    if (probe_unique_process_id(read, system_eprocess,
                                result.layout.active_process_links, offset,
                                options, result.probes)) {
      result.layout.unique_process_id = offset;
      have_pid = true;
      break;
    }
  }
  if (!have_pid) return result;

  // 4. DirectoryTableBase: page-aligned, capped, translates to 'MZ'.
  bool have_dtb = false;
  for (std::size_t offset = 0; offset + 8 <= options.max_layout_scan_bytes;
       offset += 8) {
    std::uint64_t candidate = 0;
    if (!read_scalar<std::uint64_t>(read, system_eprocess + offset, candidate)) {
      continue;
    }
    if (probe_directory_table_base(read, candidate, image_virtual_base, options,
                                   result.probes)) {
      result.layout.directory_table_base = offset;
      have_dtb = true;
      break;
    }
  }
  if (!have_dtb) return result;

  result.ok = true;
  return result;
}

/// Walk the ActiveProcessLinks list from the System EPROCESS, invoking
/// ``visit(pid, eprocess)`` for every entry, starting with the System
/// process itself (the list head), then following Flink until the walk
/// returns to the head (circular list) or the guard fires. Stops early when
/// ``visit`` returns false. This is the consumer of the derived layout -
/// the whole point is that no hardcoded offsets remain.
template <typename ReadFn, typename Visit>
int walk_process_list(ReadFn read,
                      std::uint64_t system_eprocess,
                      const EprocessLayout& layout,
                      const DeriveOptions& options,
                      Visit visit) {
  if (layout.active_process_links == 0) return 0;
  const std::uint64_t head_link = system_eprocess + layout.active_process_links;
  std::uint64_t link = head_link;
  int visited = 0;
  while (visited < options.list_walk_guard) {
    const std::uint64_t eprocess = link - layout.active_process_links;
    if (layout.unique_process_id != 0) {
      std::uint32_t pid = 0;
      if (!read_scalar<std::uint32_t>(read, eprocess + layout.unique_process_id, pid)) {
        break;
      }
      ++visited;
      if (!visit(pid, eprocess)) break;
    } else {
      ++visited;
      if (!visit(0, eprocess)) break;
    }
    if (!read_scalar<std::uint64_t>(read, eprocess + layout.active_process_links, link)) {
      break;
    }
    if (link == head_link) break;  // completed the circle
  }
  return visited;
}

}  // namespace real::kernel::derive
