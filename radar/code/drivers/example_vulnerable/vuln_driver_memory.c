#include "vuln_driver.h"

// Read target VA into kernel/stack buffer while already attached.
// `kbuf` MUST be kernel memory (pool or stack), never a caller usermode VA.
NTSTATUS ReadProcessMemoryAttached(PVOID target_addr, PVOID kbuf, ULONG size) {
    NTSTATUS status = STATUS_SUCCESS;
    __try {
        ProbeForRead(target_addr, size, 1);
        RtlCopyMemory(kbuf, target_addr, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
    }
    return status;
}

// VIRT_READ: stage through kernel pool so caller output_buffer is only
// written after KeUnstackDetachProcess (caller address space restored).
NTSTATUS ReadProcessMemoryRaw(HANDLE pid, PVOID target_addr,
                                     PVOID caller_buf, ULONG size) {
    if (size == 0 || !caller_buf || !target_addr)
        return STATUS_INVALID_PARAMETER;

    PVOID kbuf = ExAllocatePoolWithTag(PagedPool, size, 'dRvG');
    if (!kbuf) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(kbuf, size);

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId(pid, &process);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(kbuf, 'dRvG');
        return status;
    }

    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);
    // ATTACHED: only target_addr (target AS) and kbuf (kernel) are valid.
    status = ReadProcessMemoryAttached(target_addr, kbuf, size);
    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(process);
    process = NULL;

    if (NT_SUCCESS(status)) {
        __try {
            // DETACHED: caller_buf is valid again in the calling process.
            ProbeForWrite(caller_buf, size, 1);
            RtlCopyMemory(caller_buf, kbuf, size);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status = STATUS_ACCESS_VIOLATION;
        }
    }

    ExFreePoolWithTag(kbuf, 'dRvG');
    return status;
}

// VIRT_WRITE: snapshot caller payload into kernel first, then write into
// target while attached. Never read caller VA under target CR3.
NTSTATUS WriteProcessMemoryRaw(HANDLE pid, PVOID target_addr,
                                      PVOID caller_buf, ULONG size) {
    if (size == 0 || !caller_buf || !target_addr)
        return STATUS_INVALID_PARAMETER;

    PVOID kbuf = ExAllocatePoolWithTag(PagedPool, size, 'dWvG');
    if (!kbuf) return STATUS_INSUFFICIENT_RESOURCES;

    NTSTATUS status = STATUS_SUCCESS;
    __try {
        // DETACHED (caller AS): capture source payload.
        ProbeForRead(caller_buf, size, 1);
        RtlCopyMemory(kbuf, caller_buf, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ExFreePoolWithTag(kbuf, 'dWvG');
        return STATUS_ACCESS_VIOLATION;
    }

    PEPROCESS process = NULL;
    status = PsLookupProcessByProcessId(pid, &process);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(kbuf, 'dWvG');
        return status;
    }

    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);
    __try {
        // ATTACHED: only target_addr and kbuf.
        ProbeForWrite(target_addr, size, 1);
        RtlCopyMemory(target_addr, kbuf, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
    }
    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(process);

    ExFreePoolWithTag(kbuf, 'dWvG');
    return status;
}

// ═══════════════════════════════════════════════════════════════════════
// Helpers: ntoskrnl base + PE export + pattern scan (for notify arrays)
// ═══════════════════════════════════════════════════════════════════════

NTSTATUS GdrvPhysRead(PVOID user_buffer, ULONG in_len, ULONG out_len) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(in_len);

    GDRV_PHYS_REQ req;
    __try {
        ProbeForRead(user_buffer, sizeof(req), 1);
        RtlCopyMemory(&req, user_buffer, sizeof(req));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG size = min(req.size, out_len);
    size = min(size, (ULONG)GDRV_MAX_PHYS_XFER);
    if (size == 0) return STATUS_BUFFER_TOO_SMALL;

    PHYSICAL_ADDRESS pa;
    pa.QuadPart = (LONGLONG)req.phys_addr;
    PVOID mapped = MmMapIoSpace(pa, size, MmNonCached);
    if (!mapped) return STATUS_UNSUCCESSFUL;

    NTSTATUS status = STATUS_SUCCESS;
    __try {
        ProbeForWrite(user_buffer, size, 1);
        RtlCopyMemory(user_buffer, mapped, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_INVALID_PARAMETER;
    }
    MmUnmapIoSpace(mapped, size);

    DBG_PRINT("[gdrv] PHYS_READ: 0x%llx -> %lu bytes\n",
              (unsigned long long)pa.QuadPart, size);
    return status;
}

__declspec(code_seg("PAGE"))
NTSTATUS GdrvPhysWrite(PVOID user_buffer, ULONG in_len, ULONG out_len) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(out_len);

    GDRV_PHYS_REQ req;
    ULONG req_size = sizeof(req);
    if (in_len < req_size) return STATUS_BUFFER_TOO_SMALL;

    __try {
        ProbeForRead(user_buffer, req_size, 1);
        RtlCopyMemory(&req, user_buffer, req_size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG write_size = min(req.size, in_len - req_size);
    write_size = min(write_size, (ULONG)GDRV_MAX_PHYS_XFER);
    if (write_size == 0) return STATUS_BUFFER_TOO_SMALL;

    PHYSICAL_ADDRESS pa;
    pa.QuadPart = (LONGLONG)req.phys_addr;
    PVOID mapped = MmMapIoSpace(pa, write_size, MmNonCached);
    if (!mapped) return STATUS_UNSUCCESSFUL;

    NTSTATUS status = STATUS_SUCCESS;
    __try {
        RtlCopyMemory(mapped, (PUCHAR)user_buffer + req_size, write_size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
    }
    MmUnmapIoSpace(mapped, write_size);

    DBG_PRINT("[gdrv] PHYS_WRITE: 0x%llx <- %lu bytes\n",
              (unsigned long long)pa.QuadPart, write_size);
    return status;
}

__declspec(code_seg("PAGE"))
NTSTATUS GdrvVirtRead(PVOID user_buffer, ULONG in_len, ULONG out_len) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(out_len);
    if (in_len < sizeof(GDRV_VIRT_REQ)) return STATUS_BUFFER_TOO_SMALL;

    GDRV_VIRT_REQ req;
    __try {
        ProbeForRead(user_buffer, sizeof(req), 1);
        RtlCopyMemory(&req, user_buffer, sizeof(req));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG size = min(req.size, (ULONG)GDRV_MAX_VIRT_READ);
    if (size == 0) return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS status = ReadProcessMemoryRaw(
        (HANDLE)(ULONG_PTR)req.process_id,
        (PVOID)(ULONG_PTR)req.target_address,
        (PVOID)(ULONG_PTR)req.output_buffer,
        size);

    DBG_PRINT("[gdrv] VIRT_READ: pid=%llu addr=0x%llx sz=%lu\n",
              (unsigned long long)req.process_id,
              (unsigned long long)req.target_address, size);
    return status;
}

__declspec(code_seg("PAGE"))
NTSTATUS GdrvVirtWrite(PVOID user_buffer, ULONG in_len, ULONG out_len) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(out_len);
    if (in_len < sizeof(GDRV_VIRT_REQ)) return STATUS_BUFFER_TOO_SMALL;

    GDRV_VIRT_REQ req;
    ULONG req_size = sizeof(req);
    __try {
        ProbeForRead(user_buffer, req_size, 1);
        RtlCopyMemory(&req, user_buffer, req_size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG write_size = min(req.size, in_len - req_size);
    write_size = min(write_size, (ULONG)GDRV_MAX_VIRT_WRITE);
    if (write_size == 0) return STATUS_BUFFER_TOO_SMALL;

    return WriteProcessMemoryRaw(
        (HANDLE)(ULONG_PTR)req.process_id,
        (PVOID)(ULONG_PTR)req.target_address,
        (PVOID)((PUCHAR)user_buffer + req_size),
        write_size);
}

// ENTITY_WALK: attach once, walk entity list with client-supplied offsets.
