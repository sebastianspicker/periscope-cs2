#include "sim/handle_table.hpp"

namespace sim {
namespace {

constexpr std::uint32_t kVmReadBit =
    static_cast<std::uint32_t>(ac::HandleAccess::Read);

bool is_open(const HandleEntry& e) { return e.closed_tick == 0; }

}  // namespace

void HandleTable::add_handle(const HandleEntry& entry) {
  entries_.push_back(entry);
}

void HandleTable::open(std::uint32_t owner_pid, std::uint32_t target_pid,
                       std::uint32_t access_mask,
                       ac::HandleAcquisitionModel acquisition,
                       std::uint64_t tick, bool ephemeral, bool via_proxy,
                       bool hidden_during_enum) {
  HandleEntry e;
  e.owner_pid = owner_pid;
  e.target_pid = target_pid;
  e.access_mask = access_mask;
  e.acquisition = acquisition;
  e.created_tick = tick;
  e.closed_tick = 0;
  e.ephemeral = ephemeral;
  e.via_proxy = via_proxy;
  e.hidden_during_enum = hidden_during_enum;
  entries_.push_back(e);
}

void HandleTable::close_handle(std::uint32_t owner_pid, std::uint32_t target_pid,
                               std::uint64_t tick) {
  for (auto& e : entries_) {
    if (e.owner_pid == owner_pid && e.target_pid == target_pid && is_open(e)) {
      e.closed_tick = tick == 0 ? 0xFFFFFFFF : tick;
      if (e.ephemeral || (e.closed_tick != 0 && e.created_tick != 0 &&
                          e.closed_tick <= e.created_tick + 1)) {
        e.ephemeral = true;
      }
      break;
    }
  }
}

void HandleTable::close_all_from(std::uint32_t owner_pid, std::uint64_t tick) {
  for (auto& e : entries_) {
    if (e.owner_pid == owner_pid && is_open(e)) {
      e.closed_tick = tick == 0 ? 0xFFFFFFFF : tick;
    }
  }
}

std::vector<HandleEntry> HandleTable::handles_to(
    std::uint32_t target_pid) const {
  std::vector<HandleEntry> result;
  for (const auto& e : entries_) {
    if (e.target_pid == target_pid) {
      result.push_back(e);
    }
  }
  return result;
}

std::vector<HandleEntry> HandleTable::open_handles_to(
    std::uint32_t target_pid, bool include_hidden) const {
  std::vector<HandleEntry> result;
  for (const auto& e : entries_) {
    if (e.target_pid != target_pid || !is_open(e)) {
      continue;
    }
    if (!include_hidden && e.hidden_during_enum) {
      continue;
    }
    result.push_back(e);
  }
  return result;
}

std::vector<HandleEntry> HandleTable::handles_from(
    std::uint32_t owner_pid) const {
  std::vector<HandleEntry> result;
  for (const auto& e : entries_) {
    if (e.owner_pid == owner_pid) {
      result.push_back(e);
    }
  }
  return result;
}

std::vector<HandleEntry> HandleTable::open_handles_from(
    std::uint32_t owner_pid) const {
  std::vector<HandleEntry> result;
  for (const auto& e : entries_) {
    if (e.owner_pid == owner_pid && is_open(e)) {
      result.push_back(e);
    }
  }
  return result;
}

int HandleTable::unique_owners_for(std::uint32_t target_pid) const {
  std::vector<std::uint32_t> owners;
  for (const auto& e : entries_) {
    if (e.target_pid == target_pid && is_open(e)) {
      if (std::find(owners.begin(), owners.end(), e.owner_pid) == owners.end()) {
        owners.push_back(e.owner_pid);
      }
    }
  }
  return static_cast<int>(owners.size());
}

int HandleTable::open_vm_read_count(std::uint32_t target_pid) const {
  int n = 0;
  for (const auto& e : entries_) {
    if (e.target_pid == target_pid && is_open(e) &&
        (e.access_mask & kVmReadBit) != 0) {
      ++n;
    }
  }
  return n;
}

int HandleTable::ephemeral_count_for(std::uint32_t target_pid) const {
  int n = 0;
  for (const auto& e : entries_) {
    if (e.target_pid == target_pid && e.ephemeral) {
      ++n;
    }
  }
  return n;
}

int HandleTable::open_handle_count() const {
  int n = 0;
  for (const auto& e : entries_) {
    if (is_open(e)) {
      ++n;
    }
  }
  return n;
}

void HandleTable::clear() { entries_.clear(); }

}  // namespace sim
