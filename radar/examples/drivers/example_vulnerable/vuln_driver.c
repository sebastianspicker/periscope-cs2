// vuln_driver.c — DriverEntry, IRP dispatch, entity/process/module IOCTLs.

#include "vuln_driver.h"

PDEVICE_OBJECT g_devobj = NULL;
PVOID* g_pspProcessNotify = NULL;
PVOID* g_pspThreadNotify  = NULL;
PVOID* g_pspImageNotify   = NULL;
BOOLEAN g_notifyResolved  = FALSE;
BOOLEAN g_notifyResolveOk = FALSE;

NTSTATUS GdrvEntityWalk(PVOID user_buffer, ULONG in_len, ULONG out_len) {
    PAGED_CODE();
    if (in_len < sizeof(GDRV_ENTITY_WALK_REQ)) return STATUS_BUFFER_TOO_SMALL;

    GDRV_ENTITY_WALK_REQ req;
    __try {
        ProbeForRead(user_buffer, sizeof(req), 1);
        RtlCopyMemory(&req, user_buffer, sizeof(req));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_INVALID_PARAMETER;
    }

    if (req.stride == 0) return STATUS_INVALID_PARAMETER;
    if (req.count == 0 || req.count > GDRV_MAX_PLAYERS)
        req.count = GDRV_MAX_PLAYERS;

    ULONG entry_size = sizeof(GDRV_ENTITY_DATA);
    if (out_len < sizeof(GDRV_ENTITY_WALK_REQ) + entry_size)
        return STATUS_BUFFER_TOO_SMALL;

    ULONG max_results =
        (out_len - sizeof(GDRV_ENTITY_WALK_REQ)) / entry_size;
    if (max_results == 0) return STATUS_BUFFER_TOO_SMALL;
    ULONG results = min(req.count, max_results);

    // Stage ALL entity results in kernel pool. Never ProbeForWrite the
    // caller's UserBuffer while KeStackAttachProcess'd to the target.
    ULONG stage_bytes = results * entry_size;
    PGDRV_ENTITY_DATA stage = (PGDRV_ENTITY_DATA)ExAllocatePoolWithTag(
        PagedPool, stage_bytes, 'tNeG');
    if (!stage) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(stage, stage_bytes);

    PEPROCESS process;
    NTSTATUS attach_status = PsLookupProcessByProcessId(
        (HANDLE)(ULONG_PTR)req.process_id, &process);
    if (!NT_SUCCESS(attach_status)) {
        ExFreePoolWithTag(stage, 'tNeG');
        return attach_status;
    }

    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);

    ULONG success_count = 0;

    for (ULONG i = 0; i < results; ++i) {
        GDRV_ENTITY_DATA data;
        RtlZeroMemory(&data, sizeof(data));
        data.index = req.start_index + i;

        ULONG64 entry_addr = req.entity_list_addr +
            ((ULONG64)data.index * req.stride);

        ULONG64 identity_addr = 0;
        NTSTATUS status = ReadProcessMemoryAttached(
            (PVOID)(ULONG_PTR)entry_addr, &identity_addr, sizeof(identity_addr));
        if (!NT_SUCCESS(status) || !identity_addr) {
            stage[i] = data;  // keep zeroed slot for index alignment
            continue;
        }

        data.controller_addr = identity_addr;

        UCHAR team = 0;
        status = ReadProcessMemoryAttached(
            (PVOID)(ULONG_PTR)(identity_addr + req.schema_team),
            &team, sizeof(team));
        if (NT_SUCCESS(status)) data.team = team;

        ULONG64 pawn_handle = 0;
        status = ReadProcessMemoryAttached(
            (PVOID)(ULONG_PTR)(identity_addr + req.schema_pawn),
            &pawn_handle, sizeof(pawn_handle));
        if (!NT_SUCCESS(status) || !pawn_handle) {
            stage[i] = data;
            continue;
        }

        ULONG pawn_index = (ULONG)(pawn_handle & 0x7FFF);
        ULONG64 pawn_entry_addr =
            req.entity_list_addr + ((ULONG64)pawn_index * req.stride);
        ULONG64 pawn_addr = 0;
        status = ReadProcessMemoryAttached(
            (PVOID)(ULONG_PTR)pawn_entry_addr, &pawn_addr, sizeof(pawn_addr));
        if (!NT_SUCCESS(status) || !pawn_addr) {
            stage[i] = data;
            continue;
        }

        data.pawn_addr = pawn_addr;
        data.pawn_valid = 1;

        ULONG health = 0;
        status = ReadProcessMemoryAttached(
            (PVOID)(ULONG_PTR)(pawn_addr + req.schema_health),
            &health, sizeof(health));
        if (NT_SUCCESS(status)) data.health = health;

        float origin[3] = {0, 0, 0};
        status = ReadProcessMemoryAttached(
            (PVOID)(ULONG_PTR)(pawn_addr + req.schema_origin),
            &origin, sizeof(origin));
        if (NT_SUCCESS(status)) {
            data.origin_x = origin[0];
            data.origin_y = origin[1];
            data.origin_z = origin[2];
        }

        stage[i] = data;
        ++success_count;
    }

    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(process);

    // DETACHED: publish staged entities into the caller's METHOD_NEITHER buffer.
    NTSTATUS status = STATUS_SUCCESS;
    __try {
        ProbeForWrite((PUCHAR)user_buffer + sizeof(GDRV_ENTITY_WALK_REQ),
                      stage_bytes, 1);
        RtlCopyMemory((PUCHAR)user_buffer + sizeof(GDRV_ENTITY_WALK_REQ),
                      stage, stage_bytes);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
    }

    ExFreePoolWithTag(stage, 'tNeG');

    DBG_PRINT("[gdrv] ENTITY_WALK: pid=%llu results=%lu\n",
              (unsigned long long)req.process_id, success_count);
    return status;
}

// PROCESS_SCAN: full SystemProcessInformation walk including last entry.
__declspec(code_seg("PAGE"))
NTSTATUS GdrvProcessScan(PVOID user_buffer, ULONG in_len, ULONG out_len) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(in_len);
    if (out_len < sizeof(GDRV_PROCESS_SCAN_RESULT))
        return STATUS_BUFFER_TOO_SMALL;

    GDRV_PROCESS_SCAN_RESULT result;
    RtlZeroMemory(&result, sizeof(result));
    ULONG max_procs = GDRV_MAX_PROCESSES;

    ULONG buf_size = 0x10000;
    PVOID sys_info = NULL;
    NTSTATUS status = STATUS_INFO_LENGTH_MISMATCH;

    // Grow buffer until ZwQuerySystemInformation succeeds.
    for (int attempt = 0; attempt < 8; ++attempt) {
        sys_info = ExAllocatePoolWithTag(PagedPool, buf_size, 'nPsG');
        if (!sys_info) return STATUS_INSUFFICIENT_RESOURCES;

        status = ZwQuerySystemInformation(
            GdrvSystemProcessInformation, sys_info, buf_size, &buf_size);
        if (status == STATUS_INFO_LENGTH_MISMATCH) {
            ExFreePoolWithTag(sys_info, 'nPsG');
            sys_info = NULL;
            buf_size += 0x10000;
            continue;
        }
        break;
    }

    if (!NT_SUCCESS(status) || !sys_info) {
        if (sys_info) ExFreePoolWithTag(sys_info, 'nPsG');
        return status;
    }

    PGDRV_SYSTEM_PROCESS_INFORMATION spi =
        (PGDRV_SYSTEM_PROCESS_INFORMATION)sys_info;
    ULONG count = 0;

    // Correct walk: process the current entry, then advance; include last entry
    // (NextEntryOffset == 0).
    for (;;) {
        if (count >= max_procs) break;

        GDRV_PROCESS_INFO pi;
        RtlZeroMemory(&pi, sizeof(pi));
        pi.pid = (ULONG)(ULONG_PTR)spi->UniqueProcessId;
        pi.parent_pid = (ULONG)(ULONG_PTR)spi->InheritedFromUniqueProcessId;
        pi.session_id = spi->SessionId;
        pi.base = 0;  // not available from SystemProcessInformation alone

        if (spi->ImageName.Buffer && spi->ImageName.Length > 0) {
            ULONG name_len = spi->ImageName.Length / sizeof(WCHAR);
            if (name_len > GDRV_PROCESS_NAME_CHARS - 1)
                name_len = GDRV_PROCESS_NAME_CHARS - 1;
            RtlCopyMemory(pi.name, spi->ImageName.Buffer,
                          name_len * sizeof(WCHAR));
            pi.name[name_len] = 0;
        }

        result.processes[count++] = pi;

        if (spi->NextEntryOffset == 0) break;
        spi = (PGDRV_SYSTEM_PROCESS_INFORMATION)(
            (PUCHAR)spi + spi->NextEntryOffset);
    }

    result.count = count;

    __try {
        ProbeForWrite(user_buffer, sizeof(result), 1);
        RtlCopyMemory(user_buffer, &result, sizeof(result));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
    }

    ExFreePoolWithTag(sys_info, 'nPsG');
    DBG_PRINT("[gdrv] PROCESS_SCAN: %lu processes\n", count);
    return status;
}

// MODULE_LIST: PEB->Ldr InLoadOrder walk with correct header writeback.
__declspec(code_seg("PAGE"))
NTSTATUS GdrvModuleList(PVOID user_buffer, ULONG in_len, ULONG out_len) {
    PAGED_CODE();
    if (in_len < sizeof(GDRV_MODULE_LIST_REQ))
        return STATUS_BUFFER_TOO_SMALL;
    if (out_len < sizeof(GDRV_MODULE_LIST_REQ))
        return STATUS_BUFFER_TOO_SMALL;

    GDRV_MODULE_LIST_REQ req;
    __try {
        ProbeForRead(user_buffer, sizeof(req), 1);
        RtlCopyMemory(&req, user_buffer, sizeof(req));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_INVALID_PARAMETER;
    }

    if (req.max_modules == 0 || req.max_modules > GDRV_MAX_MODULES)
        req.max_modules = GDRV_MAX_MODULES;

    ULONG maxByOut =
        (out_len > sizeof(GDRV_MODULE_LIST_REQ))
            ? (ULONG)((out_len - sizeof(GDRV_MODULE_LIST_REQ)) /
                      sizeof(GDRV_MODULE_ENTRY))
            : 0;
    if (maxByOut == 0) return STATUS_BUFFER_TOO_SMALL;
    if (req.max_modules > maxByOut) req.max_modules = maxByOut;

    // Stage module records in kernel pool; write user_buffer only after detach.
    ULONG stage_bytes = req.max_modules * sizeof(GDRV_MODULE_ENTRY);
    PGDRV_MODULE_ENTRY stage = (PGDRV_MODULE_ENTRY)ExAllocatePoolWithTag(
        PagedPool, stage_bytes, 'dLmG');
    if (!stage) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(stage, stage_bytes);

    PEPROCESS process;
    NTSTATUS status = PsLookupProcessByProcessId(
        (HANDLE)(ULONG_PTR)req.process_id, &process);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(stage, 'dLmG');
        return status;
    }

    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);

    ULONG count = 0;
    __try {
        PGDRV_PEB peb = (PGDRV_PEB)PsGetProcessPeb(process);
        if (peb && peb->Ldr) {
            PLIST_ENTRY listHead = &peb->Ldr->InLoadOrderModuleList;
            PLIST_ENTRY entry = listHead->Flink;

            while (entry && entry != listHead && count < req.max_modules) {
                PGDRV_LDR_DATA_TABLE_ENTRY ldrEntry =
                    CONTAINING_RECORD(entry, GDRV_LDR_DATA_TABLE_ENTRY,
                                      InLoadOrderLinks);

                GDRV_MODULE_ENTRY mod;
                RtlZeroMemory(&mod, sizeof(mod));
                mod.base = (ULONG64)(ULONG_PTR)ldrEntry->DllBase;
                mod.size = ldrEntry->SizeOfImage;

                if (ldrEntry->BaseDllName.Buffer &&
                    ldrEntry->BaseDllName.Length > 0) {
                    ULONG nameLen =
                        ldrEntry->BaseDllName.Length / sizeof(WCHAR);
                    if (nameLen > GDRV_MODULE_NAME_CHARS - 1)
                        nameLen = GDRV_MODULE_NAME_CHARS - 1;
                    RtlCopyMemory(mod.name, ldrEntry->BaseDllName.Buffer,
                                  nameLen * sizeof(WCHAR));
                    mod.name[nameLen] = 0;
                }

                stage[count] = mod;
                ++count;
                entry = entry->Flink;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
    }

    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(process);

    // DETACHED: write header + staged entries into caller UserBuffer.
    req.count = count;
    if (NT_SUCCESS(status)) {
        ULONG write_entries = count * sizeof(GDRV_MODULE_ENTRY);
        __try {
            ProbeForWrite(user_buffer,
                          sizeof(req) + write_entries, 1);
            RtlCopyMemory(user_buffer, &req, sizeof(req));
            if (count > 0) {
                RtlCopyMemory((PUCHAR)user_buffer + sizeof(req),
                              stage, write_entries);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status = STATUS_ACCESS_VIOLATION;
        }
    }

    ExFreePoolWithTag(stage, 'dLmG');

    DBG_PRINT("[gdrv] MODULE_LIST: pid=%llu modules=%lu\n",
              (unsigned long long)req.process_id, count);
    return status;
}

// IRP dispatch
// ═══════════════════════════════════════════════════════════════════════

__declspec(code_seg("PAGE"))
NTSTATUS DriverDeviceControl(PDEVICE_OBJECT dev_obj, PIRP irp) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(dev_obj);

    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG in_len = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG out_len = stack->Parameters.DeviceIoControl.OutputBufferLength;
    PVOID buffer = irp->UserBuffer;  // METHOD_NEITHER
    ULONG_PTR info = 0;

    DBG_PRINT("[gdrv] IRP_MJ_DEVICE_CONTROL: code=0x%08x in=%lu out=%lu\n",
              code, in_len, out_len);

    switch (code) {
    case IOCTL_GDRV_PHYS_READ:
        status = GdrvPhysRead(buffer, in_len, out_len);
        info = NT_SUCCESS(status) ? out_len : 0;
        break;
    case IOCTL_GDRV_PHYS_WRITE:
        status = GdrvPhysWrite(buffer, in_len, out_len);
        info = NT_SUCCESS(status) ? in_len : 0;
        break;
    case IOCTL_GDRV_VIRT_READ:
        status = GdrvVirtRead(buffer, in_len, out_len);
        info = NT_SUCCESS(status) ? out_len : 0;
        break;
    case IOCTL_GDRV_VIRT_WRITE:
        status = GdrvVirtWrite(buffer, in_len, out_len);
        info = NT_SUCCESS(status) ? in_len : 0;
        break;
    case IOCTL_GDRV_ENTITY_WALK:
        status = GdrvEntityWalk(buffer, in_len, out_len);
        info = NT_SUCCESS(status) ? out_len : 0;
        break;
    case IOCTL_GDRV_PROCESS_SCAN:
        status = GdrvProcessScan(buffer, in_len, out_len);
        info = NT_SUCCESS(status) ? sizeof(GDRV_PROCESS_SCAN_RESULT) : 0;
        break;
    case IOCTL_GDRV_CALLBACK_STRIP:
        status = GdrvCallbackStrip(buffer, in_len, out_len);
        info = NT_SUCCESS(status) ? sizeof(GDRV_CALLBACK_STRIP_REQ) : 0;
        break;
    case IOCTL_GDRV_MODULE_LIST:
        status = GdrvModuleList(buffer, in_len, out_len);
        info = NT_SUCCESS(status) ? out_len : 0;
        break;
    default:
        break;
    }

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = info;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS DriverCreateClose(PDEVICE_OBJECT dev, PIRP irp) {
    UNREFERENCED_PARAMETER(dev);
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// ═══════════════════════════════════════════════════════════════════════
// DriverEntry / Unload — fixed device name matching usermode path
// ═══════════════════════════════════════════════════════════════════════

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path) {
    UNREFERENCED_PARAMETER(reg_path);

    DBG_PRINT("[gdrv] === gdrv.sys v1.0.6.9 BYOVD educational driver ===\n");
    DBG_PRINT("[gdrv] Device: %ws / open as %s\n",
              GDRV_DEVICE_NAME_W, GDRV_USERMODE_PATH_A);

    UNICODE_STRING dev_name, sym_name;
    RtlInitUnicodeString(&dev_name, GDRV_DEVICE_NAME_W);
    RtlInitUnicodeString(&sym_name, GDRV_SYMLINK_NAME_W);

    NTSTATUS status = IoCreateDevice(
        driver, 0, &dev_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_devobj);
    if (!NT_SUCCESS(status)) {
        DBG_PRINT("[gdrv] IoCreateDevice failed: 0x%08x\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&sym_name, &dev_name);
    if (!NT_SUCCESS(status)) {
        DBG_PRINT("[gdrv] IoCreateSymbolicLink failed: 0x%08x\n", status);
        IoDeleteDevice(g_devobj);
        g_devobj = NULL;
        return status;
    }

    driver->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
    driver->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;
    driver->DriverUnload = DriverUnload;

    // Eager resolve notify arrays (best-effort; also done on first IOCTL).
    GdrvResolveNotifyArrays();

    DBG_PRINT("[gdrv] Loaded OK. Access via \\\\.\\gdrv\n");
    DBG_PRINT("[gdrv] IOCTLs: 0x%08X-0x%08X\n",
              IOCTL_GDRV_FIRST, IOCTL_GDRV_LAST);
    return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT driver) {
    UNREFERENCED_PARAMETER(driver);

    UNICODE_STRING sym_name;
    RtlInitUnicodeString(&sym_name, GDRV_SYMLINK_NAME_W);
    IoDeleteSymbolicLink(&sym_name);
    if (g_devobj) {
        IoDeleteDevice(g_devobj);
        g_devobj = NULL;
    }

    DBG_PRINT("[gdrv] Unloaded\n");
}
