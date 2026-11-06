#include "sim/donor.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace sim {
namespace {

std::uint64_t current_tick(const World& world) {
  return static_cast<std::uint64_t>(std::max(world.lab_match_tick, 0));
}

std::string lowercase(std::string value) {
  for (char& character : value) {
    character = static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  }
  return value;
}

double legitimacy_for(const Process& process) {
  const std::string name = lowercase(process.name);
  if (name.find("steam") != std::string::npos ||
      name.find("gameoverlay") != std::string::npos) {
    return 0.95;
  }
  if (name.find("discord") != std::string::npos) {
    return 0.85;
  }
  if (name.find("svchost") != std::string::npos ||
      name.find("explorer") != std::string::npos) {
    return 0.80;
  }
  if (name.find("obs") != std::string::npos) {
    return 0.75;
  }
  if (name.find("nvsphelper") != std::string::npos) {
    return 0.70;
  }
  return process.looks_reputable ? 0.65 : 0.30;
}

}  // namespace

DonorDiscovery::DonorDiscovery(World& world) : world_(world) {}

std::vector<DonorCandidate> DonorDiscovery::find_candidates(
    std::uint32_t game_pid) {
  std::vector<DonorCandidate> candidates;
  if (!world_.proc(game_pid)) {
    return candidates;
  }

  for (const auto& [pid, process] : world_.processes) {
    if (pid == 0 || pid == game_pid) {
      continue;
    }

    AccessMask maximum_access = AccessMask::None;
    bool has_game_handle = false;
    for (const Handle& handle : world_.handles) {
      if (handle.owner_pid == pid && handle.target_pid == game_pid) {
        has_game_handle = true;
        maximum_access = static_cast<AccessMask>(
            std::max(static_cast<std::uint32_t>(maximum_access),
                     static_cast<std::uint32_t>(handle.access)));
      }
    }
    if (!has_game_handle) {
      continue;
    }

    candidates.push_back(DonorCandidate{
        pid,
        process.name,
        true,
        static_cast<std::uint32_t>(maximum_access),
        legitimacy_for(process),
        ac::HandleAcquisitionModel::HandleDuplicate,
    });
  }
  return candidates;
}

DonorCandidate DonorDiscovery::select_best(
    const std::vector<DonorCandidate>& candidates) {
  if (candidates.empty()) {
    return {};
  }
  return *std::max_element(
      candidates.begin(), candidates.end(),
      [](const DonorCandidate& left, const DonorCandidate& right) {
        if (left.legitimacy_score != right.legitimacy_score) {
          return left.legitimacy_score < right.legitimacy_score;
        }
        return left.pid > right.pid;
      });
}

bool DonorDiscovery::discover_and_select(std::uint32_t game_pid) {
  selected_ = select_best(find_candidates(game_pid));
  if (selected_.pid == 0) {
    return false;
  }

  std::ostringstream message;
  message << "donor discovered pid=" << selected_.pid
          << " name=" << selected_.name
          << " legitimacy=" << selected_.legitimacy_score;
  world_.note(message.str());
  return true;
}

HandleTransferRecord DonorDiscovery::transfer_handle(
    std::uint32_t donor_pid, std::uint32_t game_pid, std::uint32_t cheat_pid) {
  HandleTransferRecord record;
  record.source_pid = donor_pid;
  record.target_pid = game_pid;
  record.new_owner_pid = cheat_pid;
  record.transfer_tick = current_tick(world_);

  if (!world_.proc(donor_pid) || !world_.proc(game_pid) ||
      !world_.proc(cheat_pid)) {
    return record;
  }

  for (const Handle& handle : world_.handles) {
    if (handle.owner_pid == donor_pid && handle.target_pid == game_pid) {
      record.original_access = static_cast<std::uint32_t>(handle.access);
      break;
    }
  }
  if (record.original_access == 0) {
    return record;
  }

  const std::uint32_t read_only_access =
      static_cast<std::uint32_t>(AccessMask::Query) |
      static_cast<std::uint32_t>(AccessMask::VmRead);
  record.transferred_access = record.original_access & read_only_access;
  if (record.transferred_access == 0) {
    record.transferred_access = record.original_access;
  }
  record.rights_reduced = record.transferred_access != record.original_access;

  world_.handles.push_back(Handle{
      cheat_pid,
      game_pid,
      static_cast<AccessMask>(record.transferred_access),
      false,
      false,
      false,
      true,
  });
  world_.record_handle_table(
      cheat_pid, game_pid, record.transferred_access,
      ac::HandleAcquisitionModel::HandleDuplicate, /*ephemeral=*/false,
      /*via_proxy=*/true);

  world_.handle_proxy_active = true;
  world_.handle_proxy_owner_pid = donor_pid;
  world_.handle_proxy_consumer_pid = cheat_pid;
  world_.donor_hijack_active = true;
  world_.donor_hijack_donor_pid = donor_pid;
  world_.donor_hijack_consumer_pid = cheat_pid;
  world_.donor_handle_duplicated = true;

  std::ostringstream message;
  message << "handle duplicated donor=" << donor_pid << " consumer=" << cheat_pid
          << " access=0x" << std::hex << record.transferred_access;
  world_.note(message.str());
  return record;
}

ShellcodePayload DonorDiscovery::inject_payload(
    std::uint32_t donor_pid, std::uint32_t game_pid, std::uint32_t handle_value,
    bool use_direct_syscall) {
  ShellcodePayload payload;
  payload.donor_pid = donor_pid;
  payload.game_pid = game_pid;
  payload.handle_value = handle_value;
  payload.direct_syscall = use_direct_syscall;

  Process* donor = world_.proc(donor_pid);
  if (!donor || !world_.proc(game_pid)) {
    return payload;
  }

  const bool donor_has_game_handle = std::any_of(
      world_.handles.begin(), world_.handles.end(), [&](const Handle& handle) {
        return handle.owner_pid == donor_pid && handle.target_pid == game_pid &&
               has(handle.access, AccessMask::VmRead);
      });
  if (!donor_has_game_handle) {
    return payload;
  }

  payload.payload_address = donor->base + donor->memory.size();
  payload.payload_size = use_direct_syscall ? 128 : 192;
  payload.injected = true;
  donor->has_foreign_thread = true;
  donor->thread_hijacked = true;

  world_.shellcode_donor_active = true;
  world_.shellcode_donor_pid = donor_pid;
  world_.shellcode_obfuscated = true;
  world_.shellcode_xor_key_applied = true;
  world_.apc_injection_active = !use_direct_syscall;
  if (world_.apc_injection_active) {
    ++world_.apc_injection_count;
  }

  world_.note("donor payload injected pid=" + std::to_string(donor_pid) +
              " size=" + std::to_string(payload.payload_size) +
              " syscall=" + (use_direct_syscall ? "yes" : "no"));
  return payload;
}

SharedMemorySection DonorDiscovery::create_shared_section(
    std::uint32_t creator_pid, std::uint32_t consumer_pid, std::size_t size,
    bool randomize_name) {
  SharedMemorySection section;
  section.creator_pid = creator_pid;
  section.consumer_pid = consumer_pid;
  section.size = size;
  section.name_randomized = randomize_name;
  section.carries_entity_bytes = size != 0;
  if (!world_.proc(creator_pid) || !world_.proc(consumer_pid)) {
    return section;
  }

  if (randomize_name) {
    section.name = "Local\\CS2R_" + std::to_string(creator_pid) + "_" +
                   std::to_string(consumer_pid) + "_" +
                   std::to_string(current_tick(world_));
  } else {
    section.name = "Global\\CS2RadarSharedMemory";
  }
  world_.add_section(
      SharedSection{section.name, creator_pid, consumer_pid, section.carries_entity_bytes});
  world_.note("donor shared section name=" + section.name +
              " creator=" + std::to_string(creator_pid) +
              " consumer=" + std::to_string(consumer_pid));
  return section;
}

}  // namespace sim
