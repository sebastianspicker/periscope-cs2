#include "real/win/syscall_ops.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64

#include "real/win/stack_spoof.hpp"
#include "real/win/syscall_helper.hpp"

#include <cstring>

namespace real::win {

Result<HANDLE> syscall_open_process(std::uint32_t pid, ACCESS_MASK access) noexcept {
  const int ssn = syscalls().NtOpenProcess.number;
  if (ssn < 0) return Result<HANDLE>(nullptr, "NtOpenProcess SSN unresolved");

  OBJECT_ATTRIBUTES oa{};
  oa.Length = sizeof(oa);
  CLIENT_ID cid{};
  cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid));
  cid.UniqueThread = nullptr;

  HANDLE handle = nullptr;
  // Optional stack spoof around the syscall
  bool spoofed = stack_spoof_push();
  NTSTATUS st = syscall_4(ssn, reinterpret_cast<std::uint64_t>(&handle),
                          static_cast<std::uint64_t>(access),
                          reinterpret_cast<std::uint64_t>(&oa),
                          reinterpret_cast<std::uint64_t>(&cid));
  if (spoofed) stack_spoof_pop();

  if (st < 0 || !handle) {
    return Result<HANDLE>(nullptr, "NtOpenProcess syscall failed");
  }
  return handle;
}

NTSTATUS syscall_close(HANDLE h) noexcept {
  const int ssn = syscalls().NtClose.number;
  if (ssn < 0) return static_cast<NTSTATUS>(0xC00000BBL);
  return syscall_4(ssn, reinterpret_cast<std::uint64_t>(h), 0, 0, 0);
}

Result<std::size_t> syscall_read_memory(HANDLE process, std::uint64_t address,
                                        void* buffer, std::size_t size) noexcept {
  if (!process || !buffer || size == 0) {
    return Result<std::size_t>(0, "invalid args");
  }
  SIZE_T read = 0;
  NTSTATUS st = syscall_direct_NtReadVirtualMemory(
      process, reinterpret_cast<PVOID>(static_cast<uintptr_t>(address)), buffer, size,
      &read);
  if (st < 0) return Result<std::size_t>(0, "NtReadVirtualMemory syscall failed");
  return static_cast<std::size_t>(read);
}

Result<std::vector<std::uint8_t>> syscall_read_process(std::uint32_t pid,
                                                       std::uint64_t address,
                                                       std::size_t size) noexcept {
  auto h = syscall_open_process(pid, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION);
  if (!h) return Result<std::vector<std::uint8_t>>({}, h.error_msg.c_str());

  std::vector<std::uint8_t> buf(size);
  auto n = syscall_read_memory(*h, address, buf.data(), size);
  syscall_close(*h);
  if (!n) return Result<std::vector<std::uint8_t>>({}, n.error_msg.c_str());
  buf.resize(*n);
  return buf;
}

NTSTATUS syscall_query_process(HANDLE process, ULONG info_class, void* buffer,
                               ULONG buffer_size, ULONG* return_length) noexcept {
  const int ssn = syscalls().NtQueryInformationProcess.number;
  if (ssn < 0) return static_cast<NTSTATUS>(0xC00000BBL);
  return syscall_5(ssn, reinterpret_cast<std::uint64_t>(process),
                   static_cast<std::uint64_t>(info_class),
                   reinterpret_cast<std::uint64_t>(buffer),
                   static_cast<std::uint64_t>(buffer_size),
                   reinterpret_cast<std::uint64_t>(return_length));
}

}  // namespace real::win

#endif
