// backend.cpp -- IMemoryBackend integration for the educational DMA lab.
//
// LESSON (read before using):
// Every DMA operation documented here leaves a detectable forensic scar.
// The purpose of this code is to TEACH what those scars are so that
// anti-cheat developers can detect and mitigate them.
//
// Attach tries ordered real transports and fails with Unavailable when no
// hardware or privilege is present. Detach always cleans half-open state.

#include "real/dma/dma_backend.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <utility>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#elif LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace real::dma {

namespace {

// Probe paths tried for FPGA open (Linux + Windows).
const char* const kFpgaProbePaths[] = {
#if LR_PLATFORM_WINDOWS
    "\\\\.\\PCILeech",
    "\\\\.\\FPGA0",
    "\\\\.\\FTD3XX",
    "\\\\.\\rawudp",
#else
    "/dev/fpga0",
    "/dev/pcileech",
    "/dev/xdma0_c2h_0",
    "/dev/xdma0_user",
#endif
};

}  // namespace

RealDmaBackend::RealDmaBackend(bool use_fpga) : use_fpga_(use_fpga) {
#ifndef NDEBUG
  std::printf("[dma] RealDmaBackend: use_fpga=%d\n", use_fpga_ ? 1 : 0);
#endif
}

ac::Status RealDmaBackend::attach(std::uint32_t target_id) {
  // Clean any previous half-open state before re-attach.
  detach();
  target_id_ = target_id;

  // 1) Physical memory transport — probe a safe low page (typically
  //    reserved/BIOS area; success proves the path is open).
  {
    auto result = physmem_read(0x1000, 64);
    if (result) {
      attached_ = true;
      transport_ = Transport::PhysMem;
#ifndef NDEBUG
      std::printf("[dma] RealDmaBackend: attached via physical memory\n");
#endif
      return ac::Status::Ok;
    }
  }

  // 2) FPGA / PCILeech-style device — always attempt when requested, and
  //    also auto-probe common device nodes even if use_fpga_ is false so
  //    the multi-transport chain is complete when hardware is present.
  {
    const bool try_fpga = use_fpga_ || true;  // always probe; fail soft
    if (try_fpga) {
      for (const char* path : kFpgaProbePaths) {
        auto device = fpga_dma_open(path);
        if (device) {
          fpga_dev_ = std::move(*device);
          attached_ = true;
          transport_ = Transport::Fpga;
#ifndef NDEBUG
          std::printf("[dma] RealDmaBackend: attached via FPGA (%s)\n", path);
#endif
          return ac::Status::Ok;
        }
      }
    }
  }

  // 3) Thunderbolt DMA — availability check then a tiny BAR-backed probe.
  {
    auto tb = thunderbolt_available();
    if (tb && *tb) {
      auto probe = thunderbolt_dma_read(0x1000, 16);
      if (probe) {
        attached_ = true;
        transport_ = Transport::Thunderbolt;
#ifndef NDEBUG
        std::printf("[dma] RealDmaBackend: attached via Thunderbolt\n");
#endif
        return ac::Status::Ok;
      }
      // Device present but read failed — still mark as candidate only if
      // the availability signal is strong; keep Unavailable for safety.
    }
  }

#ifndef NDEBUG
  std::printf("[dma] RealDmaBackend: attach failed (no privileged transport)\n");
#endif
  attached_ = false;
  transport_ = Transport::None;
  return ac::Status::Unavailable;
}

void RealDmaBackend::detach() {
  if (fpga_dev_.fd >= 0) {
    (void)fpga_dma_close(fpga_dev_);
  }
  fpga_dev_ = FpgaDmaDevice{};
  attached_ = false;
  transport_ = Transport::None;
  target_id_ = 0;
}

ac::ReadResult RealDmaBackend::read(const ac::ReadRequest& request) {
  if (!attached_) return {ac::Status::Denied, {}};
  if (request.size == 0) return {ac::Status::Ok, {}};

  // Prefer the transport that won attach; fall through to others so a
  // transient failure on one path can still serve the read.
  if (transport_ == Transport::Fpga || (use_fpga_ && fpga_dev_.fd >= 0)) {
    if (fpga_dev_.fd >= 0) {
      auto fpga_result =
          fpga_scatter_read(fpga_dev_, request.address, request.size);
      if (fpga_result) return {ac::Status::Ok, std::move(*fpga_result)};
    }
  }

  if (transport_ == Transport::Thunderbolt) {
    auto tb = thunderbolt_dma_read(request.address, request.size);
    if (tb) return {ac::Status::Ok, std::move(*tb)};
  }

  // PhysMem (default) and fallback for every transport.
  auto result = physmem_read(request.address, request.size);
  if (result) return {ac::Status::Ok, std::move(*result)};

  // Last resort: FPGA even if attach used another transport (device may
  // have appeared later).
  if (fpga_dev_.fd >= 0) {
    auto fpga_result =
        fpga_scatter_read(fpga_dev_, request.address, request.size);
    if (fpga_result) return {ac::Status::Ok, std::move(*fpga_result)};
  }

  auto tb = thunderbolt_dma_read(request.address, request.size);
  if (tb) return {ac::Status::Ok, std::move(*tb)};

  return {ac::Status::Denied, {}};
}

}  // namespace real::dma
