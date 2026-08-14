// ===========================================================================
// Indirect Syscall Architecture (SCH-3 compliant)
//
// REQUIREMENT: The module must NOT contain the 0F 05 (syscall) instruction
// in its own .text section.  Direct syscalls (embedding 0F 05 in user code)
// are easily detected by EDRs scanning for the syscall instruction outside
// of ntdll.dll.  This file implements *indirect* syscalls to defeat that
// detection.
//
// ARCHITECTURE
//   1. RESOLVE SSN
//      resolve_ssn("NtFoo") scans the ntdll export stub for the pattern:
//        4c 8b d1       mov r10, rcx
//        b8 XX XX XX XX mov eax, SSN
//        0f 05          syscall
//        c3             ret
//      and extracts the syscall number at offset +4.
//
//   2. FIND GADGET
//      find_syscall_gadget() locates a "syscall; ret" sequence
//      (bytes 0F 05 C3) inside ntdll's .text section.
//      This is the gadget we jump to.
//
//   3. INDIRECT CALL
//      syscall_4/5/6 wrappers set r10 = rcx, eax = SSN,
//      args in rcx/rdx/r8/r9 (and stack for arg5/arg6), then
//      execute:
//        call *%[gadget]
//      The call instruction itself lives in the cheat's code,
//      but the 0F 05 (syscall) executes from ntdll memory.
//
// WHY INDIRECT?
//   EDR hooks typically place a jmp at the start of the ntdll
//   syscall stub to redirect to their monitoring code.  Direct
//   syscalls bypass this hook entirely, but leave 0F 05 in the
//   cheat's .text section -- a strong indicator.  Indirect syscalls
//   avoid both problems:
//     - No 0F 05 in user code (opsec)
//     - syscall executes from ntdll where the EDR already
//       permits it (behavioural normalcy)
//
// STACK LAYOUT for arg5/arg6
//   Because "call *%[gadget]" pushes an 8-byte return address,
//   the caller's rsp is 8 lower than normal at syscall time.
//   Stack arguments are placed at [rsp+0x28] (arg5) and
//   [rsp+0x30] (arg6) to compensate, so the kernel sees them
//   at the expected [rsp+0x28] and [rsp+0x30] positions.
// ===========================================================================

#include "real/win/syscall_helper.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
#include <array>
#include <cstring>
#include <string>
#include <utility>

#include <intrin.h>

#include "real/win/peb_util.hpp"
#include "real/win/ssn_resolve.hpp"
#include "real/win/stack_spoof.hpp"
#include "real/win/xorstr.hpp"

namespace real::win {
namespace {

bool is_in_text_section(HMODULE module, const std::uint8_t* address) {
  const auto* base = reinterpret_cast<const std::uint8_t*>(module);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    return false;
  }
  const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
  for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section) {
    if (std::memcmp(section->Name, ".text", 5) != 0) continue;
    const auto* begin = base + section->VirtualAddress;
    const auto* end = begin + section->Misc.VirtualSize;
    return address >= begin && address < end;
  }
  return false;
}

static uintptr_t find_ntdll_base() {
  static uintptr_t cached = 0;
  if (cached) return cached;
  cached = peb::find_module("ntdll");
  return cached;
}

static const std::uint8_t* find_export_by_name(uintptr_t moduleBase,
                                                const char* name) {
  const auto* base = reinterpret_cast<const std::uint8_t*>(moduleBase);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return nullptr;
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
      base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE ||
      nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    return nullptr;
  }
  auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (dir.Size == 0 || dir.VirtualAddress == 0) return nullptr;

  const auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
      base + dir.VirtualAddress);
  const auto* names = reinterpret_cast<const DWORD*>(base + exp->AddressOfNames);
  const auto* ordinals = reinterpret_cast<const WORD*>(
      base + exp->AddressOfNameOrdinals);
  const auto* functions = reinterpret_cast<const DWORD*>(
      base + exp->AddressOfFunctions);

  for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
    const char* exportName = reinterpret_cast<const char*>(base + names[i]);
    const char* a = exportName;
    const char* b = name;
    bool eq = true;
    while (*a && *b) {
      char ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
      char cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
      if (ca != cb) { eq = false; break; }
      ++a; ++b;
    }
    if (eq && *a == *b) {
      WORD ordinal = ordinals[i];
      DWORD funcRva = functions[ordinal];
      return base + funcRva;
    }
  }
  return nullptr;
}

int resolve_ssn(const char* name) {
  // Guard against debugger presence — if BeingDebugged is set,
  // return fake SSNs so the syscall path fails safely.
  {
    auto* peb = peb::get_peb();
    if (peb && peb->BeingDebugged) return -1;
  }

  // Prefer robust Hell's/Halo's/Tartarus' Gate recovery (handles hooked stubs).
  {
    SsnResult robust = resolve_ssn_robust(name);
    if (robust.number >= 0) return robust.number;
  }

  uintptr_t ntdllBase = find_ntdll_base();
  if (ntdllBase == 0) return -1;

  const auto* address = find_export_by_name(ntdllBase, name);
  if (address == nullptr ||
      !is_in_text_section(reinterpret_cast<HMODULE>(ntdllBase), address)) {
    return -1;
  }

  // Fallback: scan up to 64 bytes for Win10 classic or Win11 instrumented stubs.
  return extract_ssn_from_stub(address, 64);
}

static void* find_syscall_gadget() {
  static void* cached = nullptr;
  if (cached) return cached;

  uintptr_t ntdllBase = find_ntdll_base();
  if (ntdllBase == 0) return nullptr;

  const auto* base = reinterpret_cast<const std::uint8_t*>(ntdllBase);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return nullptr;
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
      base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE ||
      nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    return nullptr;
  }

  // Collect all "syscall; ret" (0F 05 C3) gadgets in .text
  static void* gadgets[128];
  static int gadgetCount = 0;

  const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
  for (WORD index = 0; index < nt->FileHeader.NumberOfSections;
       ++index, ++section) {
    if (std::memcmp(section->Name, ".text", 5) != 0) continue;
    const auto* begin = base + section->VirtualAddress;
    const auto* end = begin + section->Misc.VirtualSize;
    for (const auto* p = begin; p < end - 2; ++p) {
      if (p[0] == 0x0F && p[1] == 0x05 && p[2] == 0xC3) {
        if (static_cast<size_t>(gadgetCount) <
            sizeof(gadgets) / sizeof(gadgets[0])) {
          gadgets[gadgetCount++] = const_cast<std::uint8_t*>(p);
        }
      }
    }
    break;
  }

  if (gadgetCount == 0) return nullptr;

  // Return random gadget using build-key-seeded PRNG
  uint64_t state = build::kXorKeySeed;
  for (int i = 0; i < 5; ++i)
    state = state * 0x5851F42D4C957F2DULL + 0x14057B7EF767814FULL;
  cached = gadgets[static_cast<size_t>(state % gadgetCount)];
  return cached;
}

Result<void> resolve(SyscallNumber& entry, const char* name) {
  entry.name = name;
  entry.number = resolve_ssn(name);
  if (entry.number < 0) return Result<void>(std::string("Unable to resolve syscall number for ") + name);
  return {};
}

}  // namespace

Result<void> SyscallTable::resolve_all() {
  for (const std::pair<SyscallNumber*, const char*> entry : {
           std::pair{&NtOpenProcess, OBF("NtOpenProcess")},
           std::pair{&NtReadVirtualMemory, OBF("NtReadVirtualMemory")},
           std::pair{&NtClose, OBF("NtClose")},
           std::pair{&NtQueryInformationProcess, OBF("NtQueryInformationProcess")},
           std::pair{&NtCreateFile, OBF("NtCreateFile")},
           std::pair{&NtDeviceIoControlFile, OBF("NtDeviceIoControlFile")},
           std::pair{&NtSuspendProcess, OBF("NtSuspendProcess")},
           std::pair{&NtResumeProcess, OBF("NtResumeProcess")},
            std::pair{&NtDuplicateObject, OBF("NtDuplicateObject")}}) {
    auto result = resolve(*entry.first, entry.second);
    if (!result) return result;
  }
  return {};
}

SyscallTable& syscalls() {
  static SyscallTable table;
  static const bool resolved = table.resolve_all().ok;
  (void)resolved;
  return table;
}

NTSTATUS syscall_direct_NtReadVirtualMemory(HANDLE process, PVOID address,
                                            PVOID buffer, SIZE_T size,
                                            PSIZE_T bytesRead) {
  const int ssn = syscalls().NtReadVirtualMemory.number;
  if (ssn < 0) return static_cast<NTSTATUS>(0xC00000BBL);
  return syscall_5(ssn,
                   reinterpret_cast<std::uint64_t>(process),
                   reinterpret_cast<std::uint64_t>(address),
                   reinterpret_cast<std::uint64_t>(buffer),
                   static_cast<std::uint64_t>(size),
                   reinterpret_cast<std::uint64_t>(bytesRead));
}

#if LR_COMPILER_GCC || LR_COMPILER_CLANG
static void* get_syscall_gadget() {
  static void* cached = nullptr;
  if (!cached) cached = find_syscall_gadget();
  return cached;
}

NTSTATUS syscall_4(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4) {
  NTSTATUS result;
  void* gadget = get_syscall_gadget();
  if (!gadget) return static_cast<NTSTATUS>(0xC00000BBL);
  std::uint64_t canary = build::kXorKeySeed ^ 0xCAFEBABE;
  uintptr_t* ret_addr = (uintptr_t*)_AddressOfReturnAddress();
  uintptr_t orig_ret = *ret_addr;
  *ret_addr ^= 0xDEADBEEFCAFEBABEULL;
  asm volatile(
      "mov %[canary], %%r15\n\t"
      "mov %[arg1], %%rcx\n\t"
      "mov %[arg2], %%rdx\n\t"
      "mov %[arg3], %%r8\n\t"
      "mov %[arg4], %%r9\n\t"
      "mov %%rcx, %%r10\n\t"
      "mov %[ssn], %%eax\n\t"
      "call *%[gadget]\n\t"
      "cmp %[canary], %%r15\n\t"
      "je 0f\n\t"
      "mov $0xC00000BB, %%eax\n\t"
      "0:"
      : "=&a"(result)
      : [ssn] "r"(ssn), [arg1] "r"(arg1), [arg2] "r"(arg2), [arg3] "r"(arg3),
        [arg4] "r"(arg4), [gadget] "r"(gadget), [canary] "r"(canary)
      : "rcx", "rdx", "r8", "r9", "r10", "r11", "r15", "memory", "cc");
  *ret_addr = orig_ret;
  return result;
}

NTSTATUS syscall_5(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5) {
  NTSTATUS result;
  void* gadget = get_syscall_gadget();
  if (!gadget) return static_cast<NTSTATUS>(0xC00000BBL);
  std::uint64_t canary = build::kXorKeySeed ^ 0xCAFEBABE;
  uintptr_t* ret_addr = (uintptr_t*)_AddressOfReturnAddress();
  uintptr_t orig_ret = *ret_addr;
  *ret_addr ^= 0xDEADBEEFCAFEBABEULL;
  asm volatile(
      "mov %[canary], %%r15\n\t"
      "sub $56, %%rsp\n\t"
      "mov %[arg5], 32(%%rsp)\n\t"
      "mov %[arg1], %%rcx\n\t"
      "mov %[arg2], %%rdx\n\t"
      "mov %[arg3], %%r8\n\t"
      "mov %[arg4], %%r9\n\t"
      "mov %%rcx, %%r10\n\t"
      "mov %[ssn], %%eax\n\t"
      "call *%[gadget]\n\t"
      "add $56, %%rsp\n\t"
      "cmp %[canary], %%r15\n\t"
      "je 0f\n\t"
      "mov $0xC00000BB, %%eax\n\t"
      "0:"
      : "=&a"(result)
      : [ssn] "r"(ssn), [arg1] "r"(arg1), [arg2] "r"(arg2), [arg3] "r"(arg3),
        [arg4] "r"(arg4), [arg5] "r"(arg5), [gadget] "r"(gadget), [canary] "r"(canary)
      : "rcx", "rdx", "r8", "r9", "r10", "r11", "r15", "memory", "cc");
  *ret_addr = orig_ret;
  return result;
}

NTSTATUS syscall_6(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                   std::uint64_t arg6) {
  NTSTATUS result;
  void* gadget = get_syscall_gadget();
  if (!gadget) return static_cast<NTSTATUS>(0xC00000BBL);
  uint64_t canary = build::kXorKeySeed ^ 0xCAFEBABE;
  uintptr_t* ret_addr = (uintptr_t*)_AddressOfReturnAddress();
  uintptr_t orig_ret = *ret_addr;
  *ret_addr ^= 0xDEADBEEFCAFEBABEULL;
  asm volatile(
      "mov %[canary], %%r15\n\t"
      "sub $56, %%rsp\n\t"
      "mov %[arg5], 32(%%rsp)\n\t"
      "mov %[arg6], 40(%%rsp)\n\t"
      "mov %[arg1], %%rcx\n\t"
      "mov %[arg2], %%rdx\n\t"
      "mov %[arg3], %%r8\n\t"
      "mov %[arg4], %%r9\n\t"
      "mov %%rcx, %%r10\n\t"
      "mov %[ssn], %%eax\n\t"
      "call *%[gadget]\n\t"
      "add $56, %%rsp\n\t"
      "cmp %[canary], %%r15\n\t"
      "je 0f\n\t"
      "mov $0xC00000BB, %%eax\n\t"
      "0:"
      : "=&a"(result)
      : [ssn] "r"(ssn), [arg1] "r"(arg1), [arg2] "r"(arg2), [arg3] "r"(arg3),
        [arg4] "r"(arg4), [arg5] "r"(arg5), [arg6] "r"(arg6), [gadget] "r"(gadget), [canary] "r"(canary)
      : "rcx", "rdx", "r8", "r9", "r10", "r11", "r15", "memory", "cc");
  *ret_addr = orig_ret;
  return result;
}

NTSTATUS syscall_7(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                   std::uint64_t arg6, std::uint64_t arg7) {
  NTSTATUS result;
  void* gadget = get_syscall_gadget();
  if (!gadget) return static_cast<NTSTATUS>(0xC00000BBL);
  uint64_t canary = build::kXorKeySeed ^ 0xCAFEBABE;
  uintptr_t* ret_addr = (uintptr_t*)_AddressOfReturnAddress();
  uintptr_t orig_ret = *ret_addr;
  *ret_addr ^= 0xDEADBEEFCAFEBABEULL;
  asm volatile(
      "mov %[canary], %%r15\n\t"
      "sub $56, %%rsp\n\t"
      "mov %[arg5], 32(%%rsp)\n\t"
      "mov %[arg6], 40(%%rsp)\n\t"
      "mov %[arg7], 48(%%rsp)\n\t"
      "mov %[arg1], %%rcx\n\t"
      "mov %[arg2], %%rdx\n\t"
      "mov %[arg3], %%r8\n\t"
      "mov %[arg4], %%r9\n\t"
      "mov %%rcx, %%r10\n\t"
      "mov %[ssn], %%eax\n\t"
      "call *%[gadget]\n\t"
      "add $56, %%rsp\n\t"
      "cmp %[canary], %%r15\n\t"
      "je 0f\n\t"
      "mov $0xC00000BB, %%eax\n\t"
      "0:"
      : "=&a"(result)
      : [ssn] "r"(ssn), [arg1] "r"(arg1), [arg2] "r"(arg2), [arg3] "r"(arg3),
        [arg4] "r"(arg4), [arg5] "r"(arg5), [arg6] "r"(arg6), [arg7] "r"(arg7),
        [gadget] "r"(gadget), [canary] "r"(canary)
      : "rcx", "rdx", "r8", "r9", "r10", "r11", "r15", "memory", "cc");
  *ret_addr = orig_ret;
  return result;
}

NTSTATUS syscall_direct_NtDuplicateObject(HANDLE SourceProcessHandle,
                                          HANDLE SourceHandle,
                                          HANDLE TargetProcessHandle,
                                          PHANDLE TargetHandle,
                                          ACCESS_MASK DesiredAccess,
                                          ULONG HandleAttributes,
                                          ULONG Options) {
  const int ssn = syscalls().NtDuplicateObject.number;
  if (ssn < 0) return static_cast<NTSTATUS>(0xC00000BBL);
  return syscall_7(ssn,
                   reinterpret_cast<std::uint64_t>(SourceProcessHandle),
                   reinterpret_cast<std::uint64_t>(SourceHandle),
                   reinterpret_cast<std::uint64_t>(TargetProcessHandle),
                   reinterpret_cast<std::uint64_t>(TargetHandle),
                   static_cast<std::uint64_t>(DesiredAccess),
                   static_cast<std::uint64_t>(HandleAttributes),
                   static_cast<std::uint64_t>(Options));
}
#else
// MSVC — indirect syscall via MASM assembly (syscall_msvc.asm)
extern "C" {
    void* g_syscall_gadget_ptr = nullptr;
    NTSTATUS syscall_asm_4(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                           std::uint64_t arg3, std::uint64_t arg4);
    NTSTATUS syscall_asm_5(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                           std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5);
    NTSTATUS syscall_asm_6(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                           std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                           std::uint64_t arg6);
    NTSTATUS syscall_asm_7(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                           std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                           std::uint64_t arg6, std::uint64_t arg7);
}

static bool init_syscall_gadget_msvc() {
    if (!g_syscall_gadget_ptr) {
        g_syscall_gadget_ptr = find_syscall_gadget();
    }
    return g_syscall_gadget_ptr != nullptr;
}

NTSTATUS syscall_4(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4) {
    if (!init_syscall_gadget_msvc()) return static_cast<NTSTATUS>(0xC00000BBL);
    uintptr_t* ret_addr = (uintptr_t*)_AddressOfReturnAddress();
    uintptr_t orig_ret = *ret_addr;
    *ret_addr ^= 0xDEADBEEFCAFEBABEULL;
    NTSTATUS result = syscall_asm_4(ssn, arg1, arg2, arg3, arg4);
    *ret_addr = orig_ret;
    return result;
}

NTSTATUS syscall_5(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5) {
    if (!init_syscall_gadget_msvc()) return static_cast<NTSTATUS>(0xC00000BBL);
    uintptr_t* ret_addr = (uintptr_t*)_AddressOfReturnAddress();
    uintptr_t orig_ret = *ret_addr;
    *ret_addr ^= 0xDEADBEEFCAFEBABEULL;
    NTSTATUS result = syscall_asm_5(ssn, arg1, arg2, arg3, arg4, arg5);
    *ret_addr = orig_ret;
    return result;
}

NTSTATUS syscall_6(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                   std::uint64_t arg6) {
    if (!init_syscall_gadget_msvc()) return static_cast<NTSTATUS>(0xC00000BBL);
    uintptr_t* ret_addr = (uintptr_t*)_AddressOfReturnAddress();
    uintptr_t orig_ret = *ret_addr;
    *ret_addr ^= 0xDEADBEEFCAFEBABEULL;
    NTSTATUS result = syscall_asm_6(ssn, arg1, arg2, arg3, arg4, arg5, arg6);
    *ret_addr = orig_ret;
    return result;
}

NTSTATUS syscall_7(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                   std::uint64_t arg6, std::uint64_t arg7) {
    if (!init_syscall_gadget_msvc()) return static_cast<NTSTATUS>(0xC00000BBL);
    uintptr_t* ret_addr = (uintptr_t*)_AddressOfReturnAddress();
    uintptr_t orig_ret = *ret_addr;
    *ret_addr ^= 0xDEADBEEFCAFEBABEULL;
    NTSTATUS result = syscall_asm_7(ssn, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    *ret_addr = orig_ret;
    return result;
}

// GCC path defines this above the compiler branch; MSVC needs it here.
NTSTATUS syscall_direct_NtDuplicateObject(HANDLE SourceProcessHandle,
                                          HANDLE SourceHandle,
                                          HANDLE TargetProcessHandle,
                                          PHANDLE TargetHandle,
                                          ACCESS_MASK DesiredAccess,
                                          ULONG HandleAttributes,
                                          ULONG Options) {
  const int ssn = syscalls().NtDuplicateObject.number;
  if (ssn < 0) return static_cast<NTSTATUS>(0xC00000BBL);
  return syscall_7(ssn,
                   reinterpret_cast<std::uint64_t>(SourceProcessHandle),
                   reinterpret_cast<std::uint64_t>(SourceHandle),
                   reinterpret_cast<std::uint64_t>(TargetProcessHandle),
                   reinterpret_cast<std::uint64_t>(TargetHandle),
                   static_cast<std::uint64_t>(DesiredAccess),
                   static_cast<std::uint64_t>(HandleAttributes),
                   static_cast<std::uint64_t>(Options));
}
#endif

}  // namespace real::win
#endif  // LR_PLATFORM_WINDOWS && LR_ARCH_X64
