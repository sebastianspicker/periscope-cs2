// user_test.cpp — Full T2 BYOVD CS2 radar client for the educational gdrv stack.
//
// Self-contained under code/drivers (no external lib includes).
// Shares ABI with vuln_driver.c via gdrv_abi.h / gdrv_pack.hpp.
//
// Usage:
//   user_test.exe --info              # Driver capabilities + full IOCTL table
//   user_test.exe --list              # BYOVD IOCTL code list (all 8 codes)
//   user_test.exe --scan              # Full IOCTL test suite (needs driver)
//   user_test.exe --t2-cs2            # Read CS2 PE header via driver
//   user_test.exe --entities          # Full T2 CS2 entity walk
//   user_test.exe --processes         # List processes from kernel
//   user_test.exe --modules  <pid>    # List modules in process
//   user_test.exe --phys    0xADDR    # Read physical memory
//   user_test.exe --callback [type]   # Query kernel callbacks

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cinttypes>
#include <psapi.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <algorithm>

#include "gdrv_abi.h"
#include "gdrv_pack.hpp"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")

// Local no-op obfuscation macros (self-contained; no xorstr dependency).
#ifndef OBF
#define OBF(x) (x)
#endif
#ifndef OBFW
#define OBFW(x) (x)
#endif

// ═══════════════════════════════════════════════════════════════════════
// CS2 offsets (Periscope-validated lab defaults; XOR-obfuscated at rest)
// ═══════════════════════════════════════════════════════════════════════

struct Cs2Offsets {
    uint64_t entity_list;
    uint64_t local_player_pawn;
    uint64_t local_player_controller;
    uint64_t view_angles;
    uint64_t csgo_input;
    uint64_t c4;
    uint64_t window_width;
    uint64_t schema_health;
    uint64_t schema_team;
    uint64_t schema_origin;
    uint64_t schema_eye_angles;
    uint64_t schema_pawn_handle;
    uint64_t stride;
};

static constexpr uint64_t kOffsetKey = 0xA5A5A5A5A5A5A5A5ULL;

static Cs2Offsets g_offsets = {
    0x254EE60 ^ kOffsetKey,
    0x23A4238 ^ kOffsetKey,
    0x237EBA0 ^ kOffsetKey,
    0x23B9C78 ^ kOffsetKey,
    0x23B95F0 ^ kOffsetKey,
    0x236E678 ^ kOffsetKey,
    0x9118D0  ^ kOffsetKey,
    0x34C     ^ kOffsetKey,
    0x3E7     ^ kOffsetKey,
    0x13B8    ^ kOffsetKey,
    0x3340    ^ kOffsetKey,
    0x914     ^ kOffsetKey,
    0x70      ^ kOffsetKey,
};

static uint64_t off(uint64_t enc) { return enc ^ kOffsetKey; }

// ═══════════════════════════════════════════════════════════════════════
// Globals
// ═══════════════════════════════════════════════════════════════════════

static HANDLE g_hDevice = INVALID_HANDLE_VALUE;
static int g_ioctl_tests = 0;
static int g_ioctl_passed = 0;

static void test_ioctl(const char* name, DWORD code,
                       LPVOID in, DWORD in_size,
                       LPVOID out, DWORD out_size,
                       bool expect_success) {
    g_ioctl_tests++;
    DWORD returned = 0;
    BOOL ok = DeviceIoControl(g_hDevice, code, in, in_size, out, out_size,
                              &returned, nullptr);
    bool passed = (ok && expect_success) || (!ok && !expect_success);
    if (passed) g_ioctl_passed++;
    printf("  %-40s %s (code=0x%08lx, ret=%lu)\n",
           name, ok ? "OK" : "FAIL", (unsigned long)code, returned);
}

static bool open_driver() {
    g_hDevice = CreateFileA(GDRV_USERMODE_PATH_A,
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        printf("[!] %s not found (error %lu)\n",
               GDRV_USERMODE_PATH_A, GetLastError());
        printf("[!] Load driver: sc start %s\n", GDRV_SERVICE_NAME_A);
        printf("[!] Install:     sc create %s type=kernel "
               "binPath=C:\\Windows\\System32\\drivers\\vuln_driver.sys\n",
               GDRV_SERVICE_NAME_A);
        return false;
    }
    printf("[*] Driver opened: handle=0x%llx path=%s\n",
           (unsigned long long)(uintptr_t)g_hDevice, GDRV_USERMODE_PATH_A);
    return true;
}

static void close_driver() {
    if (g_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hDevice);
        g_hDevice = INVALID_HANDLE_VALUE;
    }
}

static DWORD find_cs2_pid() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = {sizeof(PROCESSENTRY32W)};
    if (Process32FirstW(snap, &pe)) {
        do {
            for (int i = 0; pe.szExeFile[i]; ++i) {
                wchar_t c = pe.szExeFile[i];
                if (c >= L'A' && c <= L'Z')
                    pe.szExeFile[i] = static_cast<wchar_t>(c + 32);
            }
            if (wcsstr(pe.szExeFile, L"cs2.exe")) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

static uint64_t get_process_base(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                           FALSE, pid);
    if (!h) return 0;
    HMODULE mods[1024];
    DWORD needed = 0;
    uint64_t base = 0;
    if (EnumProcessModules(h, mods, sizeof(mods), &needed) && needed >= sizeof(HMODULE)) {
        base = (uint64_t)(uintptr_t)mods[0];
    }
    CloseHandle(h);
    return base;
}

// ═══════════════════════════════════════════════════════════════════════
// --info / --list (no driver required)
// ═══════════════════════════════════════════════════════════════════════

static void cmd_info() {
    printf("\n=== gdrv.sys BYOVD Driver Information ===\n");
    printf("Driver:    vuln_driver.sys (gdrv.sys v1.0.6.9 pattern)\n");
    printf("CVE:       CVE-2020-15368 (arbitrary physical memory access)\n");
    printf("Dispatch:  METHOD_NEITHER\n");
    printf("Device:    %s\n", GDRV_USERMODE_PATH_A);
    printf("Service:   %s\n\n", GDRV_SERVICE_NAME_A);

    size_t n = 0;
    const GDRV_IOCTL_DESC* table = gdrv_ioctl_table(&n);
    printf("IOCTL Table (%zu entries, 0x%08X-0x%08X):\n",
           n, IOCTL_GDRV_FIRST, IOCTL_GDRV_LAST);
    for (size_t i = 0; i < n; ++i) {
        printf("  0x%08X  %-14s %s\n",
               table[i].code, table[i].name, table[i].capability);
    }

    printf("\nT2 BYOVD vs T0 RPM:\n");
    printf("  T0: OpenProcess + RPM       -> VM_READ HANDLE VISIBLE\n");
    printf("  T2: gdrv.sys IOCTL           -> NO HANDLE, only device IOCTL\n");
    printf("  Blue sees NOTHING from the radar process toward CS2.\n\n");

    printf("CS2 Offsets (Periscope-validated lab defaults):\n");
    printf("  dwEntityList            = 0x%llx\n",
           (unsigned long long)off(g_offsets.entity_list));
    printf("  dwLocalPlayerPawn       = 0x%llx\n",
           (unsigned long long)off(g_offsets.local_player_pawn));
    printf("  m_iHealth (C_BaseEntity)= 0x%llx\n",
           (unsigned long long)off(g_offsets.schema_health));
    printf("  m_iTeamNum              = 0x%llx\n",
           (unsigned long long)off(g_offsets.schema_team));
    printf("  m_vOldOrigin            = 0x%llx\n",
           (unsigned long long)off(g_offsets.schema_origin));
    printf("  m_hPlayerPawn           = 0x%llx\n",
           (unsigned long long)off(g_offsets.schema_pawn_handle));
    printf("  stride                  = 0x%llx\n",
           (unsigned long long)off(g_offsets.stride));

    // Touch pack helpers so --info exercises shipped ABI packing.
    auto phys = gdrv::pack_phys_req(0xE0000, 64);
    auto virt = gdrv::pack_virt_req(4, 0x7FFE0000, 16, 0);
    printf("\nABI pack smoke: phys_req=%zu bytes virt_req=%zu bytes device=%s\n",
           phys.size(), virt.size(), gdrv::usermode_device_path());
}

static void cmd_list() {
    printf("\n=== Known BYOVD IOCTL Codes ===\n");
    printf("%-20s %-16s %s\n", "Driver", "IOCTL", "Capability");
    printf("%-20s %-16s %s\n", "------", "----", "----------");

    size_t n = 0;
    const GDRV_IOCTL_DESC* table = gdrv_ioctl_table(&n);
    for (size_t i = 0; i < n; ++i) {
        printf("%-20s 0x%08X    %s\n", "gdrv.sys", table[i].code, table[i].name);
    }

    // Additional catalog samples (not this driver's codes)
    printf("%-20s 0x9C40A424    PROCESS_READ\n", "mhyprot3.sys");
    printf("%-20s 0x83002000    MSR/EC ACCESS\n", "EneIo64.sys");
    printf("%-20s 0x222000      PORT I/O\n", "winio.sys");
    printf("%-20s 0x80202040    PHYS_MEM\n", "aswArPot.sys");

    printf("\nThis stack IOCTL range: 0x%08X .. 0x%08X (%u codes)\n",
           IOCTL_GDRV_FIRST, IOCTL_GDRV_LAST, (unsigned)IOCTL_GDRV_COUNT);
    for (unsigned i = 0; i < IOCTL_GDRV_COUNT; ++i) {
        printf("  [%u] 0x%08X\n", i, gdrv::ioctl_code_at(i));
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Commands that need a loaded driver
// ═══════════════════════════════════════════════════════════════════════

static void cmd_phys(uint64_t address) {
    printf("\n=== Physical Memory Read ===\n");
    printf("Address: 0x%llx\n", (unsigned long long)address);
    if (!open_driver()) return;

    auto packed = gdrv::pack_phys_req(address, 64);
    uint8_t buf[1024] = {};
    test_ioctl(OBF("PHYS_READ (MmMapIoSpace)"),
               IOCTL_GDRV_PHYS_READ,
               packed.data(), (DWORD)packed.size(),
               buf, sizeof(buf), true);

    printf("Data (first 32 bytes): ");
    for (int i = 0; i < 32; i++) printf("%02x ", buf[i]);
    printf("\n");
    close_driver();
}

static void cmd_t2_cs2() {
    printf("\n=== T2 BYOVD: CS2 Memory Read (No Handle!) ===\n");

    DWORD pid = find_cs2_pid();
    if (!pid) {
        printf("[!] CS2 not found. Start cs2.exe.\n");
        return;
    }
    printf("[*] CS2 found: PID=%u\n", pid);

    uint64_t base = get_process_base(pid);
    if (!base) {
        printf("[!] Could not get CS2 base address.\n");
        return;
    }
    printf("[*] CS2 base: 0x%llx\n", (unsigned long long)base);

    if (!open_driver()) return;

    printf("[*] Reading CS2 PE header via VIRT_READ IOCTL...\n");

    uint8_t pe_header[4096] = {};
    auto packed = gdrv::pack_virt_req(
        pid, base, sizeof(pe_header),
        (uint64_t)(uintptr_t)pe_header);

    test_ioctl(OBF("VIRT_READ via KeStackAttachProcess"),
               IOCTL_GDRV_VIRT_READ,
               packed.data(), (DWORD)packed.size(),
               pe_header, sizeof(pe_header), true);

    if (pe_header[0] == 'M' && pe_header[1] == 'Z') {
        printf("\n[*] PE header verified: 'MZ' magic at 0x%llx\n",
               (unsigned long long)base);
        uint32_t pe_offset = *reinterpret_cast<uint32_t*>(pe_header + 0x3C);
        if (pe_offset + 0x54 < sizeof(pe_header)) {
            uint16_t section_count =
                *reinterpret_cast<uint16_t*>(pe_header + pe_offset + 6);
            uint32_t image_size =
                *reinterpret_cast<uint32_t*>(pe_header + pe_offset + 0x50);
            printf("[*] PE sections: %u\n", section_count);
            printf("[*] Image size:  0x%x (%u bytes)\n", image_size, image_size);
        }
        printf("[*] Entity list:  0x%llx\n",
               (unsigned long long)(base + off(g_offsets.entity_list)));
        printf("[*] Local pawn:   0x%llx\n",
               (unsigned long long)(base + off(g_offsets.local_player_pawn)));
    } else {
        printf("\n[!] PE header NOT found.\n");
    }

    printf("\n=== Educational Summary ===\n");
    printf("Technique:  KeStackAttachProcess (via IOCTL 0x%08X)\n",
           IOCTL_GDRV_VIRT_READ);
    printf("Handle:     NONE — no OpenProcess to cs2.exe for the read path\n");
    printf("Blue sees:  Device IOCTL on %s\n", GDRV_USERMODE_PATH_A);
    printf("Mitigation: DSE + HVCI + BYOVD blocklist\n");
    close_driver();
}

static void cmd_entities() {
    printf("\n=== T2 BYOVD: CS2 Entity List Walk ===\n");

    DWORD pid = find_cs2_pid();
    if (!pid) {
        printf("[!] CS2 not found. Start cs2.exe.\n");
        return;
    }
    printf("[*] CS2 PID: %u\n", pid);

    uint64_t base = get_process_base(pid);
    if (!base) {
        printf("[!] Cannot resolve CS2 base address.\n");
        return;
    }
    uint64_t entity_list = base + off(g_offsets.entity_list);
    printf("[*] Entity list address: 0x%llx\n",
           (unsigned long long)entity_list);

    if (!open_driver()) return;

    auto packed = gdrv::pack_entity_walk_req(
        pid, entity_list,
        off(g_offsets.schema_health),
        off(g_offsets.schema_team),
        off(g_offsets.schema_origin),
        off(g_offsets.schema_pawn_handle),
        0, 64, off(g_offsets.stride));

    std::vector<uint8_t> out_buf(GDRV_ENTITY_WALK_BYTES(64), 0);
    if (packed.size() <= out_buf.size()) {
        std::memcpy(out_buf.data(), packed.data(), packed.size());
    }

    test_ioctl("ENTITY_WALK (T2 radar)",
               IOCTL_GDRV_ENTITY_WALK,
               packed.data(), (DWORD)packed.size(),
               out_buf.data(), (DWORD)out_buf.size(), true);

    auto* entities = reinterpret_cast<GDRV_ENTITY_DATA*>(
        out_buf.data() + sizeof(GDRV_ENTITY_WALK_REQ));

    printf("\n%-4s %-6s %-6s %-12s %-12s %-12s\n",
           "Idx", "Health", "Team", "Origin.X", "Origin.Y", "Origin.Z");
    printf("---- ------ ------ ------------ ------------ ------------\n");

    int player_count = 0;
    int alive_count = 0;
    for (int i = 0; i < 64; ++i) {
        if (entities[i].health > 0 && entities[i].health <= 100) {
            ++player_count;
            if (entities[i].team == 3 || entities[i].team == 2) ++alive_count;
            printf("%-4u %-6u %-6u %-12.1f %-12.1f %-12.1f",
                   entities[i].index, entities[i].health, entities[i].team,
                   entities[i].origin_x, entities[i].origin_y,
                   entities[i].origin_z);
            if (entities[i].pawn_valid)
                printf(" [pawn:0x%llx]",
                       (unsigned long long)entities[i].pawn_addr);
            printf("\n");
        }
    }

    printf("\n=== Summary ===\n");
    printf("Total players: %d\n", player_count);
    printf("Alive:         %d\n", alive_count);
    printf("Technique:     KeStackAttachProcess (T2 BYOVD)\n");
    printf("Device:        %s\n", GDRV_USERMODE_PATH_A);
    close_driver();
}

static void cmd_processes() {
    printf("\n=== Kernel-Mode Process Enumeration ===\n");
    if (!open_driver()) return;

    GDRV_PROCESS_SCAN_RESULT result{};
    test_ioctl("PROCESS_SCAN",
               IOCTL_GDRV_PROCESS_SCAN, nullptr, 0,
               &result, sizeof(result), true);

    printf("\n%-8s %-8s %-32s %-10s\n", "PID", "PPID", "Name", "Session");
    printf("-------- -------- -------------------------------- ----------\n");

    for (uint32_t i = 0; i < result.count && i < GDRV_MAX_PROCESSES; ++i) {
        wchar_t wname[GDRV_PROCESS_NAME_CHARS + 1] = {};
        for (int c = 0; c < GDRV_PROCESS_NAME_CHARS; ++c)
            wname[c] = static_cast<wchar_t>(result.processes[i].name[c]);
        printf("%-8u %-8u %-32ls %-10u\n",
               result.processes[i].pid,
               result.processes[i].parent_pid,
               wname,
               result.processes[i].session_id);
    }

    printf("\nTotal: %u processes\n", result.count);
    close_driver();
}

static void cmd_modules(uint32_t pid) {
    printf("\n=== Module List via Driver ===\n");
    printf("PID: %u\n", pid);
    if (!open_driver()) return;

    std::vector<uint8_t> buf(GDRV_MODULE_LIST_BYTES(GDRV_MAX_MODULES), 0);
    auto packed = gdrv::pack_module_list_req(pid, GDRV_MAX_MODULES);
    std::memcpy(buf.data(), packed.data(), packed.size());

    test_ioctl("MODULE_LIST",
               IOCTL_GDRV_MODULE_LIST,
               buf.data(), (DWORD)sizeof(GDRV_MODULE_LIST_REQ),
               buf.data(), (DWORD)buf.size(), true);

    auto* req = reinterpret_cast<GDRV_MODULE_LIST_REQ*>(buf.data());
    uint32_t count = req->count;
    printf("\nModules: %u\n\n", count);

    auto* entries = reinterpret_cast<GDRV_MODULE_ENTRY*>(
        buf.data() + sizeof(GDRV_MODULE_LIST_REQ));
    for (uint32_t i = 0; i < count && i < GDRV_MAX_MODULES; ++i) {
        wchar_t wname[GDRV_MODULE_NAME_CHARS + 1] = {};
        for (int c = 0; c < GDRV_MODULE_NAME_CHARS; ++c)
            wname[c] = static_cast<wchar_t>(entries[i].name[c]);
        printf("  0x%llx +0x%x %ls\n",
               (unsigned long long)entries[i].base,
               entries[i].size, wname);
    }
    close_driver();
}

static void cmd_callback(int type) {
    printf("\n=== Kernel Callback Query ===\n");
    const char* type_names[] = {
        "Process creation", "Thread creation", "Image load", "Registry"
    };
    if (type < 0 || type > 3) {
        printf("Invalid type. Use 0=process, 1=thread, 2=image, 3=registry\n");
        return;
    }
    if (!open_driver()) return;

    auto packed = gdrv::pack_callback_strip_req(
        static_cast<uint32_t>(type), GDRV_CB_ACTION_QUERY);
    GDRV_CALLBACK_STRIP_REQ req{};
    std::memcpy(&req, packed.data(), sizeof(req));

    test_ioctl("CALLBACK_STRIP (query)",
               IOCTL_GDRV_CALLBACK_STRIP,
               &req, sizeof(req), &req, sizeof(req), true);

    printf("\n%s callbacks: %u registered (status_hint=%u)\n",
           type_names[type], req.count, req.status_hint);
    printf("Actions: 0=query, 1=remove_all, 2=count — against REAL notify arrays\n");
    close_driver();
}

static void cmd_scan() {
    printf("\n=== BYOVD IOCTL Test Suite ===\n");
    if (!open_driver()) return;

    uint8_t buf[2048] = {};
    auto preq = gdrv::pack_phys_req(0xE0000, 64);
    test_ioctl("PHYS_READ 0xE0000 (BIOS)",
               IOCTL_GDRV_PHYS_READ,
               preq.data(), (DWORD)preq.size(), buf, sizeof(buf), true);

    preq = gdrv::pack_phys_req(0xF0000, 64);
    test_ioctl("PHYS_READ 0xF0000 (ACPI)",
               IOCTL_GDRV_PHYS_READ,
               preq.data(), (DWORD)preq.size(), buf, sizeof(buf), true);

    DWORD pid = find_cs2_pid();
    if (pid) {
        uint64_t base = get_process_base(pid);
        if (base) {
            auto vreq = gdrv::pack_virt_req(
                pid, base, sizeof(buf), (uint64_t)(uintptr_t)buf);
            test_ioctl("VIRT_READ CS2 PE (no handle!)",
                       IOCTL_GDRV_VIRT_READ,
                       vreq.data(), (DWORD)vreq.size(),
                       buf, sizeof(buf), true);

            auto ereq = gdrv::pack_entity_walk_req(
                pid, base + off(g_offsets.entity_list),
                off(g_offsets.schema_health),
                off(g_offsets.schema_team),
                off(g_offsets.schema_origin),
                off(g_offsets.schema_pawn_handle),
                0, 4, off(g_offsets.stride));
            std::vector<uint8_t> entity_buf(GDRV_ENTITY_WALK_BYTES(4), 0);
            test_ioctl("ENTITY_WALK CS2 (T2 radar)",
                       IOCTL_GDRV_ENTITY_WALK,
                       ereq.data(), (DWORD)ereq.size(),
                       entity_buf.data(), (DWORD)entity_buf.size(), true);
        } else {
            printf("[!] Cannot resolve CS2 base — skipping VIRT/ENTITY tests\n");
        }
    } else {
        printf("[!] CS2 not running — skipping VIRT/ENTITY tests\n");
    }

    GDRV_PROCESS_SCAN_RESULT scan_result{};
    test_ioctl("PROCESS_SCAN (kernel enum)",
               IOCTL_GDRV_PROCESS_SCAN, nullptr, 0,
               &scan_result, sizeof(scan_result), true);
    printf("  Processes found: %u\n", scan_result.count);

    auto creq_bytes = gdrv::pack_callback_strip_req(0, GDRV_CB_ACTION_QUERY);
    GDRV_CALLBACK_STRIP_REQ creq{};
    std::memcpy(&creq, creq_bytes.data(), sizeof(creq));
    test_ioctl("CALLBACK_STRIP (query process)",
               IOCTL_GDRV_CALLBACK_STRIP,
               &creq, sizeof(creq), &creq, sizeof(creq), true);
    printf("  Process callbacks: %u (hint=%u)\n", creq.count, creq.status_hint);

    printf("\nResults: %d/%d IOCTL tests passed\n",
           g_ioctl_passed, g_ioctl_tests);

    printf("\n=== T2 BYOVD Lesson ===\n");
    printf("Handle table:  ZERO handles to cs2.exe from radar for IOCTL path\n");
    printf("Evidence:      Device handle %s only\n", GDRV_USERMODE_PATH_A);
    printf("Blue detects:  Driver blocklist, device watch, IOCTL monitoring\n");
    printf("Mitigation:    DSE + HVCI + callback integrity\n");
    close_driver();
}

static void print_usage(const char* prog) {
    printf("gdrv.sys BYOVD Testing Tool\n");
    printf("Usage:\n");
    printf("  %s --info                Driver capabilities + CS2 offsets\n", prog);
    printf("  %s --list                BYOVD IOCTL code list (all 8)\n", prog);
    printf("  %s --scan                Full IOCTL test suite\n", prog);
    printf("  %s --phys    0xADDR      Read physical memory\n", prog);
    printf("  %s --t2-cs2              Read CS2 PE header via driver\n", prog);
    printf("  %s --entities            Full T2 CS2 entity walk\n", prog);
    printf("  %s --processes           List processes from kernel\n", prog);
    printf("  %s --modules  <pid>      List modules in process\n", prog);
    printf("  %s --callback [0-3]      Query kernel callbacks\n", prog);
    printf("\n--info and --list do not require a loaded driver.\n");
    printf("Device path: %s\n", GDRV_USERMODE_PATH_A);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--info") == 0)
        cmd_info();
    else if (strcmp(argv[1], "--list") == 0)
        cmd_list();
    else if (strcmp(argv[1], "--scan") == 0)
        cmd_scan();
    else if (strcmp(argv[1], "--phys") == 0 && argc > 2)
        cmd_phys(strtoull(argv[2], nullptr, 16));
    else if (strcmp(argv[1], "--t2-cs2") == 0)
        cmd_t2_cs2();
    else if (strcmp(argv[1], "--entities") == 0)
        cmd_entities();
    else if (strcmp(argv[1], "--processes") == 0)
        cmd_processes();
    else if (strcmp(argv[1], "--modules") == 0 && argc > 2)
        cmd_modules(static_cast<uint32_t>(strtoul(argv[2], nullptr, 10)));
    else if (strcmp(argv[1], "--callback") == 0) {
        int type = (argc > 2) ? atoi(argv[2]) : 0;
        cmd_callback(type);
    } else {
        printf("Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}
