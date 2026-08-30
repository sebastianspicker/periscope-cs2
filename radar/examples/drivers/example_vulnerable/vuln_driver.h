// vuln_driver.h — Shared state/API for educational gdrv multi-TU split.
#pragma once

// vuln_driver.c — Full gdrv.sys v1.0.6.9 BYOVD driver + T2 extensions.
//
// MATCHES REVERSE-ENGINEERED gdrv.sys (Gigabyte) v1.0.6.9
// CVE-2020-15368: arbitrary physical memory read/write via IOCTL
//
// Extensions: ENTITY_WALK, PROCESS_SCAN, CALLBACK_STRIP (real notify arrays),
//             MODULE_LIST (PEB LDR walk).
//
// Device path (must match usermode CreateFile):
//   \Device\gdrv  /  \DosDevices\gdrv  /  \\.\gdrv
//
// Shared ABI: gdrv_abi.h

#include <ntddk.h>
#include <ntintsafe.h>
#include <ntstrsafe.h>

// Prefer WDK PE image helpers when present; fall back to local minimal defs.
#if defined(__has_include)
#  if __has_include(<ntimage.h>)
#    include <ntimage.h>
#    define GDRV_HAS_NTIMAGE 1
#  endif
#endif

#include "gdrv_abi.h"

#ifndef DBG
#define DBG_PRINT(...)
#else
#define DBG_PRINT DbgPrint
#endif

// ═══════════════════════════════════════════════════════════════════════
// Minimal PE image structures (when ntimage.h is unavailable)
// ═══════════════════════════════════════════════════════════════════════

#ifndef GDRV_HAS_NTIMAGE
#ifndef IMAGE_DOS_SIGNATURE
#define IMAGE_DOS_SIGNATURE 0x5A4D
#endif
#ifndef IMAGE_NT_SIGNATURE
#define IMAGE_NT_SIGNATURE 0x00004550
#endif
#ifndef IMAGE_DIRECTORY_ENTRY_EXPORT
#define IMAGE_DIRECTORY_ENTRY_EXPORT 0
#endif

typedef struct _GDRV_IMAGE_DOS_HEADER {
    USHORT e_magic;
    USHORT e_cblp;
    USHORT e_cp;
    USHORT e_crlc;
    USHORT e_cparhdr;
    USHORT e_minalloc;
    USHORT e_maxalloc;
    USHORT e_ss;
    USHORT e_sp;
    USHORT e_csum;
    USHORT e_ip;
    USHORT e_cs;
    USHORT e_lfarlc;
    USHORT e_ovno;
    USHORT e_res[4];
    USHORT e_oemid;
    USHORT e_oeminfo;
    USHORT e_res2[10];
    LONG   e_lfanew;
} GDRV_IMAGE_DOS_HEADER;

typedef struct _GDRV_IMAGE_DATA_DIRECTORY {
    ULONG VirtualAddress;
    ULONG Size;
} GDRV_IMAGE_DATA_DIRECTORY;

typedef struct _GDRV_IMAGE_OPTIONAL_HEADER64 {
    USHORT Magic;
    UCHAR  MajorLinkerVersion;
    UCHAR  MinorLinkerVersion;
    ULONG  SizeOfCode;
    ULONG  SizeOfInitializedData;
    ULONG  SizeOfUninitializedData;
    ULONG  AddressOfEntryPoint;
    ULONG  BaseOfCode;
    ULONGLONG ImageBase;
    ULONG  SectionAlignment;
    ULONG  FileAlignment;
    USHORT MajorOperatingSystemVersion;
    USHORT MinorOperatingSystemVersion;
    USHORT MajorImageVersion;
    USHORT MinorImageVersion;
    USHORT MajorSubsystemVersion;
    USHORT MinorSubsystemVersion;
    ULONG  Win32VersionValue;
    ULONG  SizeOfImage;
    ULONG  SizeOfHeaders;
    ULONG  CheckSum;
    USHORT Subsystem;
    USHORT DllCharacteristics;
    ULONGLONG SizeOfStackReserve;
    ULONGLONG SizeOfStackCommit;
    ULONGLONG SizeOfHeapReserve;
    ULONGLONG SizeOfHeapCommit;
    ULONG  LoaderFlags;
    ULONG  NumberOfRvaAndSizes;
    GDRV_IMAGE_DATA_DIRECTORY DataDirectory[16];
} GDRV_IMAGE_OPTIONAL_HEADER64;

typedef struct _GDRV_IMAGE_FILE_HEADER {
    USHORT Machine;
    USHORT NumberOfSections;
    ULONG  TimeDateStamp;
    ULONG  PointerToSymbolTable;
    ULONG  NumberOfSymbols;
    USHORT SizeOfOptionalHeader;
    USHORT Characteristics;
} GDRV_IMAGE_FILE_HEADER;

typedef struct _GDRV_IMAGE_NT_HEADERS64 {
    ULONG Signature;
    GDRV_IMAGE_FILE_HEADER FileHeader;
    GDRV_IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} GDRV_IMAGE_NT_HEADERS64;

typedef struct _GDRV_IMAGE_EXPORT_DIRECTORY {
    ULONG Characteristics;
    ULONG TimeDateStamp;
    USHORT MajorVersion;
    USHORT MinorVersion;
    ULONG Name;
    ULONG Base;
    ULONG NumberOfFunctions;
    ULONG NumberOfNames;
    ULONG AddressOfFunctions;
    ULONG AddressOfNames;
    ULONG AddressOfNameOrdinals;
} GDRV_IMAGE_EXPORT_DIRECTORY;

#define IMAGE_DOS_HEADER            GDRV_IMAGE_DOS_HEADER
#define PIMAGE_DOS_HEADER           GDRV_IMAGE_DOS_HEADER*
#define IMAGE_NT_HEADERS64          GDRV_IMAGE_NT_HEADERS64
#define PIMAGE_NT_HEADERS64         GDRV_IMAGE_NT_HEADERS64*
#define IMAGE_DATA_DIRECTORY        GDRV_IMAGE_DATA_DIRECTORY
#define IMAGE_EXPORT_DIRECTORY      GDRV_IMAGE_EXPORT_DIRECTORY
#define PIMAGE_EXPORT_DIRECTORY     GDRV_IMAGE_EXPORT_DIRECTORY*
#endif

static __inline int GdrvStrEq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        ++a;
        ++b;
    }
    return *a == *b;
}

// ═══════════════════════════════════════════════════════════════════════
// Undocumented / incomplete kernel types (not always in public WDK headers)
// ═══════════════════════════════════════════════════════════════════════

typedef enum _GDRV_SYSTEM_INFORMATION_CLASS {
    GdrvSystemProcessInformation = 5,
    GdrvSystemModuleInformation  = 11
} GDRV_SYSTEM_INFORMATION_CLASS;

typedef struct _GDRV_SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
} GDRV_SYSTEM_PROCESS_INFORMATION, *PGDRV_SYSTEM_PROCESS_INFORMATION;

typedef struct _GDRV_RTL_PROCESS_MODULE_INFORMATION {
    HANDLE Section;
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR FullPathName[256];
} GDRV_RTL_PROCESS_MODULE_INFORMATION, *PGDRV_RTL_PROCESS_MODULE_INFORMATION;

typedef struct _GDRV_RTL_PROCESS_MODULES {
    ULONG NumberOfModules;
    GDRV_RTL_PROCESS_MODULE_INFORMATION Modules[1];
} GDRV_RTL_PROCESS_MODULES, *PGDRV_RTL_PROCESS_MODULES;

// Minimal PEB / LDR for attached-process module walk
typedef struct _GDRV_PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} GDRV_PEB_LDR_DATA, *PGDRV_PEB_LDR_DATA;

typedef struct _GDRV_LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} GDRV_LDR_DATA_TABLE_ENTRY, *PGDRV_LDR_DATA_TABLE_ENTRY;

typedef struct _GDRV_PEB {
    BOOLEAN InheritedAddressSpace;
    BOOLEAN ReadImageFileExecOptions;
    BOOLEAN BeingDebugged;
    BOOLEAN BitField;
    PVOID Mutant;
    PVOID ImageBaseAddress;
    PGDRV_PEB_LDR_DATA Ldr;
} GDRV_PEB, *PGDRV_PEB;

// EX_CALLBACK slot used by Psp*NotifyRoutine arrays (Windows x64)
typedef struct _GDRV_EX_CALLBACK_ROUTINE_BLOCK {
    EX_RUNDOWN_REF RundownProtect;
    PVOID Function;   // actual notify routine (after decode)
    PVOID Context;
} GDRV_EX_CALLBACK_ROUTINE_BLOCK, *PGDRV_EX_CALLBACK_ROUTINE_BLOCK;

NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

NTKERNELAPI PPEB NTAPI PsGetProcessPeb(PEPROCESS Process);

// ═══════════════════════════════════════════════════════════════════════
// Device globals
// ═══════════════════════════════════════════════════════════════════════


// Device / notify globals (defined in vuln_driver.c)
extern PDEVICE_OBJECT g_devobj;
extern PVOID* g_pspProcessNotify;
extern PVOID* g_pspThreadNotify;
extern PVOID* g_pspImageNotify;
extern BOOLEAN g_notifyResolved;
extern BOOLEAN g_notifyResolveOk;
extern LIST_ENTRY* g_cmCallbackListHead;

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;
DRIVER_DISPATCH DriverCreateClose;
DRIVER_DISPATCH DriverDeviceControl;

// Memory path
NTSTATUS ReadProcessMemoryAttached(PVOID target_addr, PVOID kbuf, ULONG size);
NTSTATUS ReadProcessMemoryRaw(HANDLE pid, PVOID target_addr, PVOID caller_buf, ULONG size);
NTSTATUS WriteProcessMemoryRaw(HANDLE pid, PVOID target_addr, PVOID caller_buf, ULONG size);
NTSTATUS GdrvPhysRead(PVOID user_buffer, ULONG in_len, ULONG out_len);
NTSTATUS GdrvPhysWrite(PVOID user_buffer, ULONG in_len, ULONG out_len);
NTSTATUS GdrvVirtRead(PVOID user_buffer, ULONG in_len, ULONG out_len);
NTSTATUS GdrvVirtWrite(PVOID user_buffer, ULONG in_len, ULONG out_len);

// Notify / callback strip
NTSTATUS GdrvGetNtoskrnlInfo(PVOID* outBase, PULONG outSize);
PVOID GdrvFindExport(PVOID moduleBase, const char* exportName);
PVOID* GdrvFindNotifyArrayNear(PVOID start, ULONG scanLen);
VOID GdrvResolveNotifyArrays(VOID);
PVOID GdrvDecodeCallbackSlot(PVOID raw);
ULONG GdrvCountNotifyArray(PVOID* arr);
ULONG GdrvStripNotifyArray(PVOID* arr);
LIST_ENTRY* GdrvFindCmCallbackList(VOID);
ULONG GdrvCountCmCallbacks(VOID);
ULONG GdrvStripCmCallbacks(VOID);
NTSTATUS GdrvCallbackStrip(PVOID user_buffer, ULONG in_len, ULONG out_len);

// IOCTL handlers in main TU
NTSTATUS GdrvEntityWalk(PVOID user_buffer, ULONG in_len, ULONG out_len);
NTSTATUS GdrvProcessScan(PVOID user_buffer, ULONG in_len, ULONG out_len);
NTSTATUS GdrvModuleList(PVOID user_buffer, ULONG in_len, ULONG out_len);
