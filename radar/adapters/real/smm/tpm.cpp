// tpm.cpp — TPM PCR operations backend (Linux /dev/tpm*, Windows TBS).
//
// TECHNIQUE: TPM2_PCR_Read / PCR_Extend / PCR_Event via device or TBS.
//
// SCAR: Abnormal PCR values fail remote attestation.
//
// BLUE: Quote + nonce attestation; compare PCR banks to golden set.
//
// MITIGATION: Discrete TPM, measured boot policy, no PCR reset post-boot.

#include "real/smm/smm_interface.hpp"
#include "real/platform.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#elif LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#endif

namespace real::smm {
namespace {

#pragma pack(push, 1)
struct Tpm2CmdHeader {
  std::uint16_t tag = 0x8001;
  std::uint32_t command_size = 0;
  std::uint32_t command_code = 0;
};

struct Tpm2PcrSelection {
  std::uint32_t count = 1;
  std::uint16_t hash = 0x000B;
  std::uint8_t sizeof_select = 3;
  std::uint8_t pcr_select[3]{};
};

struct Tpm2PcrReadCmd {
  Tpm2CmdHeader header;
  Tpm2PcrSelection selection;
};
#pragma pack(pop)

constexpr std::uint32_t kTpm2CcPcrRead = 0x0000017E;
constexpr std::uint32_t kTpm2CcPcrExtend = 0x00000182;
constexpr std::uint32_t kTpm2CcPcrEvent = 0x0000013F;

bool select_pcr(Tpm2PcrSelection& selection, int pcr_index) {
  if (pcr_index < 0 || pcr_index >= 24) return false;
  std::memset(selection.pcr_select, 0, sizeof(selection.pcr_select));
  selection.pcr_select[pcr_index / 8] =
      static_cast<std::uint8_t>(1u << (pcr_index % 8));
  return true;
}

void append_be16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 8));
  out.push_back(static_cast<std::uint8_t>(value));
}

void append_be32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 24));
  out.push_back(static_cast<std::uint8_t>(value >> 16));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
  out.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t read_be32(const std::uint8_t* bytes) {
  return (static_cast<std::uint32_t>(bytes[0]) << 24) |
         (static_cast<std::uint32_t>(bytes[1]) << 16) |
         (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

std::uint16_t digest_size_for_alg(std::uint16_t hash_alg) {
  switch (hash_alg) {
    case 0x0004:
      return 20;
    case 0x000B:
      return 32;
    case 0x000C:
      return 48;
    case 0x000D:
      return 64;
    default:
      return 0;
  }
}

Result<std::vector<std::uint8_t>> parse_pcr_read_response(
    const std::vector<std::uint8_t>& response) {
  if (response.size() < 10) {
    return Result<std::vector<std::uint8_t>>({}, "TPM response too short");
  }
  const std::uint32_t response_code = read_be32(response.data() + 6);
  if (response_code != 0) {
    char error[64];
    std::snprintf(error, sizeof(error), "TPM error code: 0x%08x", response_code);
    return Result<std::vector<std::uint8_t>>({}, error);
  }
  std::size_t offset = 10 + 4;  // header + updateCounter
  if (response.size() < offset + 4) {
    return Result<std::vector<std::uint8_t>>({}, "TPM response too short");
  }
  const std::uint32_t selection_count = read_be32(response.data() + offset);
  offset += 4;
  for (std::uint32_t i = 0; i < selection_count; ++i) {
    if (response.size() < offset + 3) {
      return Result<std::vector<std::uint8_t>>({}, "invalid TPM PCR selection");
    }
    const std::uint8_t select_size = response[offset + 2];
    offset += 3 + select_size;
    if (response.size() < offset) {
      return Result<std::vector<std::uint8_t>>({}, "invalid TPM PCR selection");
    }
  }
  if (response.size() < offset + 6) {
    return Result<std::vector<std::uint8_t>>({}, "TPM response missing PCR digest");
  }
  const std::uint32_t digest_count = read_be32(response.data() + offset);
  offset += 4;
  if (digest_count < 1) {
    return Result<std::vector<std::uint8_t>>({}, "unexpected TPM PCR digest count");
  }
  const std::uint16_t hash_alg = static_cast<std::uint16_t>(
      (response[offset] << 8) | response[offset + 1]);
  const std::uint16_t digest_size = digest_size_for_alg(hash_alg);
  if (!digest_size) {
    return Result<std::vector<std::uint8_t>>(
        {}, "Unknown TPM hash algorithm: 0x" + std::to_string(hash_alg));
  }
  offset += 2;
  if (response.size() < offset + digest_size) {
    return Result<std::vector<std::uint8_t>>({}, "invalid TPM PCR digest");
  }
  return std::vector<std::uint8_t>(response.begin() + static_cast<std::ptrdiff_t>(offset),
                                   response.begin() +
                                       static_cast<std::ptrdiff_t>(offset + digest_size));
}

std::vector<std::uint8_t> build_pcr_read_command(int pcr_index) {
  auto bswap32 = [](std::uint32_t v) {
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
  };
  auto bswap16 = [](std::uint16_t v) {
    return static_cast<std::uint16_t>((v >> 8) | (v << 8));
  };
  Tpm2PcrReadCmd command{};
  command.header.tag = bswap16(0x8001);
  command.header.command_size = bswap32(static_cast<std::uint32_t>(sizeof(command)));
  command.header.command_code = bswap32(kTpm2CcPcrRead);
  command.selection.count = bswap32(1);
  command.selection.hash = bswap16(0x000B);
  select_pcr(command.selection, pcr_index);
  std::vector<std::uint8_t> bytes(sizeof(command));
  std::memcpy(bytes.data(), &command, sizeof(command));
  return bytes;
}

#if LR_PLATFORM_LINUX
int open_tpm() {
  int fd = open("/dev/tpmrm0", O_RDWR);
  return fd >= 0 ? fd : open("/dev/tpm0", O_RDWR);
}

Result<std::vector<std::uint8_t>> submit_tpm_command(
    const std::vector<std::uint8_t>& command) {
  int fd = open_tpm();
  if (fd < 0) {
    auto error = os_error("open /dev/tpm0 (need root or tpm group)");
    return Result<std::vector<std::uint8_t>>({}, error.error_msg);
  }
  if (write(fd, command.data(), command.size()) !=
      static_cast<ssize_t>(command.size())) {
    close(fd);
    auto error = os_error("TPM command write failed");
    return Result<std::vector<std::uint8_t>>({}, error.error_msg);
  }
  std::vector<std::uint8_t> response(4096);
  const ssize_t count = read(fd, response.data(), response.size());
  close(fd);
  if (count <= 0) return Result<std::vector<std::uint8_t>>({}, "TPM read failed");
  response.resize(static_cast<std::size_t>(count));
  return response;
}
#endif

#if LR_PLATFORM_WINDOWS
struct TbsContextParams2 {
  std::uint32_t version;
  std::uint32_t include_tpm12;
  std::uint32_t include_tpm20;
};

using TbsContextCreate = std::uint32_t(WINAPI*)(const TbsContextParams2*, void**);
using TbsSubmitCommand = std::uint32_t(WINAPI*)(void*, std::uint32_t, std::uint32_t,
                                                 const std::uint8_t*, std::uint32_t,
                                                 std::uint8_t*, std::uint32_t*);
using TbsContextClose = std::uint32_t(WINAPI*)(void*);

Result<std::vector<std::uint8_t>> submit_tpm_command_win(
    const std::vector<std::uint8_t>& command) {
  HMODULE tbs = ::LoadLibraryA("Tbs.dll");
  if (!tbs) return Result<std::vector<std::uint8_t>>({}, "TBS not available");
  auto create = reinterpret_cast<TbsContextCreate>(
      ::GetProcAddress(tbs, "Tbsi_Context_Create"));
  auto submit = reinterpret_cast<TbsSubmitCommand>(
      ::GetProcAddress(tbs, "Tbsip_Submit_Command"));
  auto close_context = reinterpret_cast<TbsContextClose>(
      ::GetProcAddress(tbs, "Tbsip_Context_Close"));
  if (!create || !submit || !close_context) {
    ::FreeLibrary(tbs);
    return Result<std::vector<std::uint8_t>>({}, "TBS functions not found");
  }
  TbsContextParams2 params{2, 0, 1};
  void* context = nullptr;
  if (create(&params, &context) != 0) {
    ::FreeLibrary(tbs);
    return Result<std::vector<std::uint8_t>>({}, "Tbsi_Context_Create failed");
  }
  std::vector<std::uint8_t> response(4096);
  std::uint32_t response_size = static_cast<std::uint32_t>(response.size());
  const std::uint32_t status =
      submit(context, 0, 200, command.data(),
             static_cast<std::uint32_t>(command.size()), response.data(),
             &response_size);
  close_context(context);
  ::FreeLibrary(tbs);
  if (status != 0) {
    return Result<std::vector<std::uint8_t>>({}, "Tbsip_Submit_Command failed");
  }
  response.resize(response_size);
  return response;
}
#endif

Result<std::vector<std::uint8_t>> submit_tpm(
    const std::vector<std::uint8_t>& command) {
#if LR_PLATFORM_LINUX
  return submit_tpm_command(command);
#elif LR_PLATFORM_WINDOWS
  return submit_tpm_command_win(command);
#else
  (void)command;
  return Result<std::vector<std::uint8_t>>({}, "TPM requires Linux or Windows");
#endif
}

}  // namespace

Result<std::vector<std::uint8_t>> tpm_read_pcr(int pcr_index) {
  std::printf("[smm] tpm_read_pcr: %d\n", pcr_index);
  if (pcr_index < 0 || pcr_index >= 24) {
    return Result<std::vector<std::uint8_t>>(
        {}, "PCR index must be between 0 and 23");
  }
  auto response = submit_tpm(build_pcr_read_command(pcr_index));
  if (!response) return response;
  auto digest = parse_pcr_read_response(*response);
  if (digest) {
    std::printf("[smm] PCR %d digest: ", pcr_index);
    for (std::uint8_t byte : *digest) std::printf("%02x", byte);
    std::printf("\n");
  }
  return digest;
}

Result<void> tpm_extend_pcr(int pcr_index, const std::vector<std::uint8_t>& data) {
  std::printf("[smm] tpm_extend_pcr: %d size=%zu\n", pcr_index, data.size());
  if (pcr_index < 0 || pcr_index >= 24) {
    return Result<void>("PCR index must be between 0 and 23");
  }
  if (data.size() != 32) {
    return Result<void>("TPM2 PCR extend requires a SHA-256 digest (32 bytes)");
  }

  std::vector<std::uint8_t> command;
  append_be16(command, 0x8002);  // TPM_ST_SESSIONS
  append_be32(command, 0);
  append_be32(command, kTpm2CcPcrExtend);
  append_be32(command, static_cast<std::uint32_t>(pcr_index));
  append_be32(command, 9);
  append_be32(command, 0x40000009);  // TPM_RS_PW
  append_be16(command, 0);
  command.push_back(0);
  append_be16(command, 0);
  append_be32(command, 1);
  append_be16(command, 0x000B);
  command.insert(command.end(), data.begin(), data.end());
  const std::uint32_t command_size = static_cast<std::uint32_t>(command.size());
  command[2] = static_cast<std::uint8_t>(command_size >> 24);
  command[3] = static_cast<std::uint8_t>(command_size >> 16);
  command[4] = static_cast<std::uint8_t>(command_size >> 8);
  command[5] = static_cast<std::uint8_t>(command_size);

  auto response = submit_tpm(command);
  if (!response) return Result<void>(response.error_msg);
  if (response->size() < 10) return Result<void>("TPM response too short");
  const std::uint32_t response_code = read_be32(response->data() + 6);
  if (response_code != 0) {
    char error[64];
    std::snprintf(error, sizeof(error), "TPM error: 0x%08x", response_code);
    return Result<void>(error);
  }
  std::printf("[smm] PCR %d extended successfully\n", pcr_index);
  return Result<void>();
}

Result<void> tpm_spoof_pcr(int pcr_index,
                            const std::vector<std::uint8_t>& fake_value) {
  std::printf("[smm] tpm_spoof_pcr: %d size=%zu\n", pcr_index, fake_value.size());
  if (pcr_index < 0 || pcr_index >= 24) return Result<void>("PCR index 0-23");
  if (fake_value.size() != 32) {
    return Result<void>("Spoof value must be SHA-256 digest (32 bytes)");
  }

  auto current = tpm_read_pcr(pcr_index);
  std::string current_hex;
  if (current) {
    for (auto b : *current) {
      char buf[4];
      std::snprintf(buf, 4, "%02x", b);
      current_hex += buf;
    }
  }

  // PCR_Event path: extends with hash of event data (cannot arbitrarily set).
  // Documents that true arbitrary set requires SMM/TPM hardware residual.
  std::vector<std::uint8_t> cmd;
  append_be16(cmd, 0x8002);
  append_be32(cmd, 0);
  append_be32(cmd, kTpm2CcPcrEvent);
  append_be32(cmd, static_cast<std::uint32_t>(pcr_index));
  append_be32(cmd, 9);
  append_be32(cmd, 0x40000009);
  append_be16(cmd, 0);
  cmd.push_back(0);
  append_be16(cmd, 0);
  append_be16(cmd, static_cast<std::uint16_t>(fake_value.size()));
  cmd.insert(cmd.end(), fake_value.begin(), fake_value.end());
  const std::uint32_t cmd_size = static_cast<std::uint32_t>(cmd.size());
  cmd[2] = static_cast<std::uint8_t>(cmd_size >> 24);
  cmd[3] = static_cast<std::uint8_t>(cmd_size >> 16);
  cmd[4] = static_cast<std::uint8_t>(cmd_size >> 8);
  cmd[5] = static_cast<std::uint8_t>(cmd_size);

  auto resp = submit_tpm(cmd);
  if (!resp) {
    // Fall back to extend for platforms that reject PCR_Event without auth.
    auto ext = tpm_extend_pcr(pcr_index, fake_value);
    if (!ext) return ext;
  } else if (resp->size() >= 10) {
    const std::uint32_t rc = read_be32(resp->data() + 6);
    if (rc != 0) {
      auto ext = tpm_extend_pcr(pcr_index, fake_value);
      if (!ext) {
        return Result<void>("TPM PCR_Event/Extend failed: 0x" +
                            std::to_string(rc));
      }
    }
  }

  auto new_val = tpm_read_pcr(pcr_index);
  std::string new_hex;
  if (new_val) {
    for (auto b : *new_val) {
      char buf[4];
      std::snprintf(buf, 4, "%02x", b);
      new_hex += buf;
    }
  }

  std::printf("[smm] PCR %d mutated via Event/Extend (not true set)\n", pcr_index);
  std::printf("[smm]   Previous: %s\n",
              current_hex.empty() ? "(unreadable)" : current_hex.c_str());
  std::printf("[smm]   Current:  %s\n",
              new_hex.empty() ? "(unreadable)" : new_hex.c_str());
  std::printf(
      "[smm] REAL SMM SPOOF residual: SMM handler rewrites TPM MMIO PCR "
      "before PCR_Read response\n");
  return Result<void>();
}

Result<std::vector<std::uint8_t>> tpm_predict_pcr_extend(
    const std::vector<std::uint8_t>& current_pcr,
    const std::vector<std::uint8_t>& extend_digest) {
  return pcr_extend_sha256(current_pcr, extend_digest);
}

}  // namespace real::smm
