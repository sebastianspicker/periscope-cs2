// gdrv_abi.h — Shared IOCTL ABI for the educational gdrv.sys BYOVD stack.
//
// Consumed by:
//   - vuln_driver.c     (kernel, WDK)
//   - user_test.cpp     (usermode client)
//   - gdrv_pack.* / host_tests.cpp  (host unit tests, no WDK)
//
// Layouts and codes match code/lib/real/kernel/vulnerable_driver.* and
// reverse-engineered gdrv.sys v1.0.6.9 (CVE-2020-15368).
//
// Device path (both sides):  \\.\gdrv
//   Kernel create:  \Device\gdrv
//   DOS symlink:    \DosDevices\gdrv
//   Usermode open:  \\.\gdrv

#pragma once

#ifdef __cplusplus
#  include <cstdint>
#  include <cstddef>
#else
#  include <stdint.h>
#  include <stddef.h>
#endif

// ── Device path (single surface, both sides) ─────────────────────────
// Kernel uses the NT object path forms; usermode uses the Win32 form.
#define GDRV_DEVICE_NAME_W        L"\\Device\\gdrv"
#define GDRV_SYMLINK_NAME_W       L"\\DosDevices\\gdrv"
#define GDRV_USERMODE_PATH_A      "\\\\.\\gdrv"
#define GDRV_USERMODE_PATH_W      L"\\\\.\\gdrv"
#define GDRV_SERVICE_NAME_A       "gdrv"
#define GDRV_SERVICE_NAME_W       L"gdrv"

// ── IOCTL codes (METHOD_NEITHER, FILE_DEVICE_UNKNOWN family) ────────
// CTL_CODE(0x22, 0x800+n, METHOD_NEITHER, FILE_ANY_ACCESS) pattern.
#define IOCTL_GDRV_PHYS_READ       0xC3502000u
#define IOCTL_GDRV_PHYS_WRITE      0xC3502004u
#define IOCTL_GDRV_VIRT_READ       0xC3502008u
#define IOCTL_GDRV_VIRT_WRITE      0xC350200Cu
#define IOCTL_GDRV_ENTITY_WALK     0xC3502010u
#define IOCTL_GDRV_PROCESS_SCAN    0xC3502014u
#define IOCTL_GDRV_CALLBACK_STRIP  0xC3502018u
#define IOCTL_GDRV_MODULE_LIST     0xC350201Cu

#define IOCTL_GDRV_FIRST           IOCTL_GDRV_PHYS_READ
#define IOCTL_GDRV_LAST            IOCTL_GDRV_MODULE_LIST
#define IOCTL_GDRV_COUNT           8

// Callback strip types / actions
#define GDRV_CB_TYPE_PROCESS       0u
#define GDRV_CB_TYPE_THREAD        1u
#define GDRV_CB_TYPE_IMAGE         2u
#define GDRV_CB_TYPE_REGISTRY      3u

#define GDRV_CB_ACTION_QUERY       0u
#define GDRV_CB_ACTION_REMOVE_ALL  1u
#define GDRV_CB_ACTION_COUNT       2u

// Limits
#define GDRV_MAX_PHYS_XFER         512u
#define GDRV_MAX_VIRT_READ         4096u
#define GDRV_MAX_VIRT_WRITE        256u
#define GDRV_MAX_PLAYERS           64u
#define GDRV_MAX_PROCESSES         256u
#define GDRV_MAX_MODULES           128u
#define GDRV_MODULE_NAME_CHARS     64u
#define GDRV_PROCESS_NAME_CHARS    32u

// Notify array capacity (Windows keeps these fixed-size EX_CALLBACK arrays)
#define GDRV_NOTIFY_ARRAY_SLOTS    64u

#pragma pack(push, 1)

// PHYS_READ / PHYS_WRITE header (gdrv.sys pattern)
typedef struct GDRV_PHYS_REQ_ {
    uint64_t phys_addr;   // +0x00 PHYSICAL_ADDRESS.QuadPart
    uint32_t size;        // +0x08
    uint32_t reserved;    // +0x0C
} GDRV_PHYS_REQ;

// VIRT_READ / VIRT_WRITE (gdrv.sys pattern)
typedef struct GDRV_VIRT_REQ_ {
    uint64_t process_id;      // +0x00
    uint64_t target_address;  // +0x08
    uint64_t output_buffer;   // +0x10 usermode pointer (METHOD_NEITHER)
    uint32_t size;            // +0x18
    uint32_t flags;           // +0x1C
} GDRV_VIRT_REQ;

// ENTITY_WALK request
typedef struct GDRV_ENTITY_WALK_REQ_ {
    uint64_t process_id;         // +0x00 CS2 PID
    uint64_t entity_list_addr;   // +0x08 dwEntityList resolved
    uint64_t schema_health;      // +0x10 m_iHealth
    uint64_t schema_team;        // +0x18 m_iTeamNum
    uint64_t schema_origin;      // +0x20 m_vOldOrigin
    uint64_t schema_pawn;        // +0x28 m_hPlayerPawn
    uint32_t start_index;        // +0x30
    uint32_t count;              // +0x34
    uint64_t stride;             // +0x38 entity list stride
} GDRV_ENTITY_WALK_REQ;

// Per-entity result written after the request header
typedef struct GDRV_ENTITY_DATA_ {
    uint32_t index;
    uint32_t health;
    uint32_t team;
    uint32_t pawn_valid;
    float    origin_x;
    float    origin_y;
    float    origin_z;
    uint64_t controller_addr;
    uint64_t pawn_addr;
} GDRV_ENTITY_DATA;

// PROCESS_SCAN
typedef struct GDRV_PROCESS_INFO_ {
    uint32_t pid;
    uint32_t parent_pid;
    uint16_t name[GDRV_PROCESS_NAME_CHARS];  // UTF-16, not wchar_t (kernel vs host)
    uint64_t base;
    uint32_t session_id;
} GDRV_PROCESS_INFO;

typedef struct GDRV_PROCESS_SCAN_RESULT_ {
    uint32_t count;
    GDRV_PROCESS_INFO processes[GDRV_MAX_PROCESSES];
} GDRV_PROCESS_SCAN_RESULT;

// CALLBACK_STRIP
typedef struct GDRV_CALLBACK_STRIP_REQ_ {
    uint32_t callback_type;  // GDRV_CB_TYPE_*
    uint32_t action;         // GDRV_CB_ACTION_*
    uint32_t count;          // out: number found / removed
    uint32_t status_hint;    // out: 0=ok resolved, 1=pattern miss, 2=unsupported type
} GDRV_CALLBACK_STRIP_REQ;

// MODULE_LIST header; module records follow contiguously
typedef struct GDRV_MODULE_LIST_REQ_ {
    uint64_t process_id;
    uint32_t max_modules;
    uint32_t count;  // output: modules written
} GDRV_MODULE_LIST_REQ;

// One module record (packed after GDRV_MODULE_LIST_REQ)
typedef struct GDRV_MODULE_ENTRY_ {
    uint64_t base;
    uint32_t size;
    uint32_t pad;  // keep 8-byte alignment for name start on some hosts
    uint16_t name[GDRV_MODULE_NAME_CHARS];
} GDRV_MODULE_ENTRY;

#pragma pack(pop)

// ── Compile-time size checks (host + kernel when C++/static_assert avail) ──
#ifdef __cplusplus
static_assert(sizeof(GDRV_PHYS_REQ) == 16, "GDRV_PHYS_REQ size");
static_assert(sizeof(GDRV_VIRT_REQ) == 32, "GDRV_VIRT_REQ size");
static_assert(sizeof(GDRV_ENTITY_WALK_REQ) == 64, "GDRV_ENTITY_WALK_REQ size");
static_assert(sizeof(GDRV_ENTITY_DATA) == 44, "GDRV_ENTITY_DATA size");
static_assert(sizeof(GDRV_CALLBACK_STRIP_REQ) == 16, "GDRV_CALLBACK_STRIP_REQ size");
static_assert(sizeof(GDRV_MODULE_LIST_REQ) == 16, "GDRV_MODULE_LIST_REQ size");
static_assert(sizeof(GDRV_MODULE_ENTRY) == 8 + 4 + 4 + GDRV_MODULE_NAME_CHARS * 2,
              "GDRV_MODULE_ENTRY size");
// PROCESS_INFO: 4+4+64+8+4 = 84
static_assert(sizeof(GDRV_PROCESS_INFO) == 84, "GDRV_PROCESS_INFO size");
#endif

// Byte size of the full process-scan result buffer
#define GDRV_PROCESS_SCAN_RESULT_BYTES  (sizeof(GDRV_PROCESS_SCAN_RESULT))

// Module list total buffer for N modules
#define GDRV_MODULE_LIST_BYTES(n) \
    (sizeof(GDRV_MODULE_LIST_REQ) + (size_t)(n) * sizeof(GDRV_MODULE_ENTRY))

// Entity walk total buffer for N entities
#define GDRV_ENTITY_WALK_BYTES(n) \
    (sizeof(GDRV_ENTITY_WALK_REQ) + (size_t)(n) * sizeof(GDRV_ENTITY_DATA))

// IOCTL table entry for --info/--list surfaces
typedef struct GDRV_IOCTL_DESC_ {
    uint32_t code;
    const char* name;
    const char* capability;
} GDRV_IOCTL_DESC;

#ifdef __cplusplus
inline const GDRV_IOCTL_DESC* gdrv_ioctl_table(size_t* out_count) {
    static const GDRV_IOCTL_DESC kTable[] = {
        {IOCTL_GDRV_PHYS_READ,      "PHYS_READ",      "MmMapIoSpace physical memory read"},
        {IOCTL_GDRV_PHYS_WRITE,     "PHYS_WRITE",     "MmMapIoSpace physical memory write"},
        {IOCTL_GDRV_VIRT_READ,      "VIRT_READ",      "KeStackAttachProcess process read"},
        {IOCTL_GDRV_VIRT_WRITE,     "VIRT_WRITE",     "KeStackAttachProcess process write"},
        {IOCTL_GDRV_ENTITY_WALK,    "ENTITY_WALK",    "T2 CS2 entity list walk"},
        {IOCTL_GDRV_PROCESS_SCAN,   "PROCESS_SCAN",   "ZwQuerySystemInformation process enum"},
        {IOCTL_GDRV_CALLBACK_STRIP, "CALLBACK_STRIP", "Notify callback query/count/remove"},
        {IOCTL_GDRV_MODULE_LIST,    "MODULE_LIST",    "PEB LDR module walk"},
    };
    if (out_count) *out_count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}
#endif
