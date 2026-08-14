// real_fwd.hpp — Single include for all real backends.
// Includes the platform layer, OS API wrappers, CS2 process attachment,
// and all hardware backends (VMX, DMA, GPU, SMM, networking).
//
// Educational note: this is the "real mode" entry point. When you include
// this header, you get access to real OS/hardware operations. The sim
// counterparts remain available via sim/ and pairs/ includes.
//
// Build with -DLR_ENABLE_REAL_ALL=ON to compile all real backends. The default
// build is simulation-only. Individual backends can be toggled with:
//   LR_ENABLE_REAL_RPM, _SYSCALL, _KERNEL, _VMX, _DMA, _SMM, _GPU, _NET
#pragma once

#include "real/error.hpp"
#include "real/memory.hpp"
#include "real/process.hpp"
#include "real/library.hpp"
#include "real/platform.hpp"
#include "real/mode/mode.hpp"

// ── OS API wrappers ────────────────────────────────────────────────
#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#endif

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
#include "real/win/syscall_helper.hpp"
#endif

// ── Hardware backends ──────────────────────────────────────────────
#if LR_ARCH_X64
#include "real/vmx/vmx_intrin.hpp"
#endif

#include "real/dma/dma_backend.hpp"
#include "real/gpu/render_pipeline.hpp"
#include "real/net/net_client.hpp"
#include "real/smm/smm_interface.hpp"

// ── CS2 process integration (primary target) ───────────────────────
#include "real/cs2/process.hpp"
#include "real/cs2/offsets.hpp"
#include "real/cs2/memory.hpp"
#include "real/cs2/entities.hpp"
#include "real/cs2/radar.hpp"
#include "real/cs2/diagnostic.hpp"
