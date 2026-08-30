// stack_spoof.hpp — Return-address / call-stack spoofing for stack walkers.
//
// RtlWalkFrameChain / CaptureStackBackTrace attribute the call to the
// first non-spoofed return address. We:
//   1. Locate a `jmp rbx` / `jmp rax` gadget inside ntdll .text
//   2. Locate legitimate `ret` gadgets for synthetic frames
//   3. Provide a spoofed-call primitive that plants a fake ntdll frame
//      before invoking a function pointer
//
// Used by indirect syscalls (syscall_helper) and stack_spoof strategy.

#pragma once

#include "real/platform.hpp"

#include <cstdint>

namespace real::win {

struct StackSpoofGadgets {
  void* jmp_rbx = nullptr;   // ntdll: jmp rbx
  void* jmp_rax = nullptr;   // ntdll: jmp rax
  void* ret_gadget = nullptr; // ntdll: ret
  void* add_rsp_ret = nullptr; // ntdll: add rsp, imm; ret (optional)
  bool valid = false;
};

/// Scan ntdll .text for spoof gadgets. Cached after first success.
StackSpoofGadgets& stack_spoof_gadgets() noexcept;

/// Plant `fake_ret` as the return address for the current frame, invoke
/// `fn(arg)`, then restore. Returns the function's return value.
/// On failure (no gadget), invokes fn directly.
std::uint64_t spoof_call_1(void* fn, std::uint64_t arg) noexcept;

/// Spoofed call with 4 integer args (x64 RCX,RDX,R8,R9).
std::uint64_t spoof_call_4(void* fn, std::uint64_t a0, std::uint64_t a1,
                           std::uint64_t a2, std::uint64_t a3) noexcept;

/// Overwrite the caller's return address with a ntdll ret gadget, save original.
/// Pair with stack_spoof_pop().
bool stack_spoof_push() noexcept;
void stack_spoof_pop() noexcept;

/// True if gadgets resolved.
bool stack_spoof_ready() noexcept;

}  // namespace real::win
