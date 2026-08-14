// host_tests.cpp — Host-side unit tests for the shipped gdrv ABI/pack surface.
//
// Drives gdrv_pack.hpp / gdrv_abi.h (the same headers used by user_test and
// the kernel driver). No parallel re-implementation of wire formats.
//
// Build:  cmake --build build --target gdrv_host_tests
// Run:    gdrv_host_tests.exe

#include "gdrv_abi.h"
#include "gdrv_pack.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static int g_failed = 0;
static int g_passed = 0;

#define EXPECT(cond, msg)                                                         \
    do {                                                                          \
        if (cond) {                                                               \
            ++g_passed;                                                           \
            std::printf("  PASS  %s\n", msg);                                     \
        } else {                                                                  \
            ++g_failed;                                                           \
            std::printf("  FAIL  %s\n", msg);                                     \
        }                                                                         \
    } while (0)

static void test_ioctl_constants() {
    std::printf("\n[test] IOCTL constants and table\n");
    EXPECT(IOCTL_GDRV_PHYS_READ == 0xC3502000u, "PHYS_READ == 0xC3502000");
    EXPECT(IOCTL_GDRV_PHYS_WRITE == 0xC3502004u, "PHYS_WRITE == 0xC3502004");
    EXPECT(IOCTL_GDRV_VIRT_READ == 0xC3502008u, "VIRT_READ == 0xC3502008");
    EXPECT(IOCTL_GDRV_VIRT_WRITE == 0xC350200Cu, "VIRT_WRITE == 0xC350200C");
    EXPECT(IOCTL_GDRV_ENTITY_WALK == 0xC3502010u, "ENTITY_WALK == 0xC3502010");
    EXPECT(IOCTL_GDRV_PROCESS_SCAN == 0xC3502014u, "PROCESS_SCAN == 0xC3502014");
    EXPECT(IOCTL_GDRV_CALLBACK_STRIP == 0xC3502018u, "CALLBACK_STRIP == 0xC3502018");
    EXPECT(IOCTL_GDRV_MODULE_LIST == 0xC350201Cu, "MODULE_LIST == 0xC350201C");
    EXPECT(IOCTL_GDRV_COUNT == 8u, "IOCTL_GDRV_COUNT == 8");

    for (unsigned i = 0; i < IOCTL_GDRV_COUNT; ++i) {
        std::uint32_t c = gdrv::ioctl_code_at(i);
        EXPECT(c == (0xC3502000u + i * 4u), "ioctl_code_at sequential stride 4");
    }

    size_t n = 0;
    const GDRV_IOCTL_DESC* table = gdrv_ioctl_table(&n);
    EXPECT(n == 8, "gdrv_ioctl_table has 8 entries");
    EXPECT(table != nullptr, "gdrv_ioctl_table non-null");
    if (table && n == 8) {
        EXPECT(table[0].code == 0xC3502000u, "table[0] PHYS_READ");
        EXPECT(table[7].code == 0xC350201Cu, "table[7] MODULE_LIST");
        EXPECT(std::strcmp(table[0].name, "PHYS_READ") == 0, "table[0].name");
        EXPECT(std::strcmp(table[7].name, "MODULE_LIST") == 0, "table[7].name");
    }
}

static void test_struct_sizes() {
    std::printf("\n[test] Struct sizes (ABI)\n");
    EXPECT(sizeof(GDRV_PHYS_REQ) == 16, "GDRV_PHYS_REQ == 16");
    EXPECT(sizeof(GDRV_VIRT_REQ) == 32, "GDRV_VIRT_REQ == 32");
    EXPECT(sizeof(GDRV_ENTITY_WALK_REQ) == 64, "GDRV_ENTITY_WALK_REQ == 64");
    EXPECT(sizeof(GDRV_ENTITY_DATA) == 44, "GDRV_ENTITY_DATA == 44");
    EXPECT(sizeof(GDRV_CALLBACK_STRIP_REQ) == 16, "GDRV_CALLBACK_STRIP_REQ == 16");
    EXPECT(sizeof(GDRV_MODULE_LIST_REQ) == 16, "GDRV_MODULE_LIST_REQ == 16");
    EXPECT(sizeof(GDRV_PROCESS_INFO) == 84, "GDRV_PROCESS_INFO == 84");
    EXPECT(sizeof(GDRV_MODULE_ENTRY) ==
               (8 + 4 + 4 + GDRV_MODULE_NAME_CHARS * 2),
           "GDRV_MODULE_ENTRY size");
}

static void test_device_path() {
    std::printf("\n[test] Device path consistency\n");
    const char* path = gdrv::usermode_device_path();
    EXPECT(path != nullptr, "usermode_device_path non-null");
    EXPECT(std::strcmp(path, "\\\\.\\gdrv") == 0, "path == \\\\.\\gdrv");
    EXPECT(std::strcmp(path, GDRV_USERMODE_PATH_A) == 0,
           "path matches GDRV_USERMODE_PATH_A");
    EXPECT(std::strcmp(GDRV_SERVICE_NAME_A, "gdrv") == 0, "service name gdrv");
}

static void test_pack_phys() {
    std::printf("\n[test] pack_phys_req (0xE0000 / 64)\n");
    const std::uint64_t addr = 0xE0000ull;
    const std::uint32_t size = 64;
    auto buf = gdrv::pack_phys_req(addr, size);
    EXPECT(buf.size() == sizeof(GDRV_PHYS_REQ), "phys packed size == 16");

    GDRV_PHYS_REQ decoded{};
    EXPECT(gdrv::decode_phys_req(buf.data(), buf.size(), &decoded),
           "decode_phys_req ok");
    EXPECT(decoded.phys_addr == addr, "phys_addr == 0xE0000");
    EXPECT(decoded.size == size, "size == 64");
    EXPECT(decoded.reserved == 0, "reserved == 0");

    // Round-trip field extraction without re-implementing pack
    GDRV_PHYS_REQ raw{};
    std::memcpy(&raw, buf.data(), sizeof(raw));
    EXPECT(raw.phys_addr == 0xE0000ull, "raw memcpy phys_addr");
    EXPECT(raw.size == 64u, "raw memcpy size");
}

static void test_pack_virt() {
    std::printf("\n[test] pack_virt_req (pid/addr/size/out)\n");
    const std::uint32_t pid = 1234;
    const std::uint64_t address = 0x7FF600001000ull;
    const std::uint32_t size = 256;
    const std::uint64_t out_buf = 0x00000000AABBCC00ull;

    auto buf = gdrv::pack_virt_req(pid, address, size, out_buf);
    EXPECT(buf.size() == sizeof(GDRV_VIRT_REQ), "virt packed size == 32");

    GDRV_VIRT_REQ decoded{};
    EXPECT(gdrv::decode_virt_req(buf.data(), buf.size(), &decoded),
           "decode_virt_req ok");
    EXPECT(decoded.process_id == pid, "process_id");
    EXPECT(decoded.target_address == address, "target_address");
    EXPECT(decoded.output_buffer == out_buf, "output_buffer");
    EXPECT(decoded.size == size, "size");
    EXPECT(decoded.flags == 0, "flags == 0");
}

static void test_pack_entity_and_module() {
    std::printf("\n[test] pack_entity_walk / pack_module_list / callback\n");
    auto ent = gdrv::pack_entity_walk_req(
        42, 0x1000, 0x34C, 0x3E7, 0x13B8, 0x914, 0, 64, 0x70);
    EXPECT(ent.size() == sizeof(GDRV_ENTITY_WALK_REQ), "entity pack size");
    GDRV_ENTITY_WALK_REQ er{};
    std::memcpy(&er, ent.data(), sizeof(er));
    EXPECT(er.process_id == 42, "entity pid");
    EXPECT(er.entity_list_addr == 0x1000, "entity list");
    EXPECT(er.schema_health == 0x34C, "health off");
    EXPECT(er.count == 64, "entity count");
    EXPECT(er.stride == 0x70, "stride");

    auto mod = gdrv::pack_module_list_req(99, 128);
    EXPECT(mod.size() == sizeof(GDRV_MODULE_LIST_REQ), "module pack size");
    GDRV_MODULE_LIST_REQ mr{};
    std::memcpy(&mr, mod.data(), sizeof(mr));
    EXPECT(mr.process_id == 99, "module pid");
    EXPECT(mr.max_modules == 128, "max_modules");
    EXPECT(mr.count == 0, "count starts 0");

    auto cb = gdrv::pack_callback_strip_req(GDRV_CB_TYPE_PROCESS,
                                            GDRV_CB_ACTION_QUERY);
    EXPECT(cb.size() == sizeof(GDRV_CALLBACK_STRIP_REQ), "callback pack size");
    GDRV_CALLBACK_STRIP_REQ cr{};
    std::memcpy(&cr, cb.data(), sizeof(cr));
    EXPECT(cr.callback_type == 0, "cb type process");
    EXPECT(cr.action == 0, "cb action query");
}

static void test_buffer_macros() {
    std::printf("\n[test] Buffer size macros\n");
    EXPECT(GDRV_ENTITY_WALK_BYTES(4) ==
               sizeof(GDRV_ENTITY_WALK_REQ) + 4 * sizeof(GDRV_ENTITY_DATA),
           "ENTITY_WALK_BYTES(4)");
    EXPECT(GDRV_MODULE_LIST_BYTES(2) ==
               sizeof(GDRV_MODULE_LIST_REQ) + 2 * sizeof(GDRV_MODULE_ENTRY),
           "MODULE_LIST_BYTES(2)");
    EXPECT(GDRV_PROCESS_SCAN_RESULT_BYTES == sizeof(GDRV_PROCESS_SCAN_RESULT),
           "PROCESS_SCAN_RESULT_BYTES");
}

static void test_attach_safe_publish() {
    std::printf("\n[test] Attach-safe stage publish (shipped gdrv:: helpers)\n");

    // VIRT_READ protocol: stage (kernel) → caller_out (only after detach)
    std::uint8_t stage[64];
    for (int i = 0; i < 64; ++i) stage[i] = static_cast<std::uint8_t>(0xA0 + i);
    std::uint8_t caller[64] = {};
    EXPECT(gdrv::virt_read_publish_stage(stage, 64, caller, sizeof(caller)),
           "virt_read_publish_stage 64");
    EXPECT(std::memcmp(stage, caller, 64) == 0, "caller receives full stage");
    EXPECT(!gdrv::virt_read_publish_stage(stage, 64, caller, 32),
           "virt_read rejects undersized caller cap");

    // ENTITY_WALK: stage array published after request header
    GDRV_ENTITY_DATA ents[2] = {};
    ents[0].index = 1;
    ents[0].health = 100;
    ents[0].team = 2;
    ents[0].origin_x = 1.5f;
    ents[1].index = 2;
    ents[1].health = 50;
    ents[1].pawn_valid = 1;
    ents[1].pawn_addr = 0xDEADBEEFCAFEULL;

    std::vector<std::uint8_t> out(GDRV_ENTITY_WALK_BYTES(2), 0);
    EXPECT(gdrv::publish_entity_stage(ents, 2, out.data(), out.size()),
           "publish_entity_stage");
    auto* pub = reinterpret_cast<GDRV_ENTITY_DATA*>(
        out.data() + sizeof(GDRV_ENTITY_WALK_REQ));
    EXPECT(pub[0].health == 100 && pub[0].index == 1, "entity[0] fields");
    EXPECT(pub[1].pawn_addr == 0xDEADBEEFCAFEULL && pub[1].pawn_valid == 1,
           "entity[1] fields");
    EXPECT(gdrv::entity_stage_bytes(2) == 2 * sizeof(GDRV_ENTITY_DATA),
           "entity_stage_bytes");

    // MODULE_LIST: header + staged entries
    GDRV_MODULE_LIST_REQ mhdr{};
    mhdr.process_id = 4242;
    mhdr.max_modules = 8;
    GDRV_MODULE_ENTRY mods[1] = {};
    mods[0].base = 0x7FF00000ULL;
    mods[0].size = 0x1000;
    mods[0].name[0] = L'n';
    mods[0].name[1] = L't';
    mods[0].name[2] = 0;
    std::vector<std::uint8_t> mout(GDRV_MODULE_LIST_BYTES(1), 0);
    EXPECT(gdrv::publish_module_stage(mhdr, mods, 1, mout.data(), mout.size()),
           "publish_module_stage");
    auto* mh = reinterpret_cast<GDRV_MODULE_LIST_REQ*>(mout.data());
    EXPECT(mh->process_id == 4242, "module hdr pid preserved");
    EXPECT(mh->count == 1, "module hdr count set");
    auto* me = reinterpret_cast<GDRV_MODULE_ENTRY*>(
        mout.data() + sizeof(GDRV_MODULE_LIST_REQ));
    EXPECT(me->base == 0x7FF00000ULL && me->size == 0x1000, "module entry");
}

// Structural check of shipped vuln_driver.c: attach-safe protocol present.
static std::string read_file_text(const char* path) {
    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0) f = nullptr;
#else
    f = std::fopen(path, "rb");
#endif
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0) {
        std::fclose(f);
        return {};
    }
    std::string s(static_cast<std::size_t>(n), '\0');
    std::fread(s.data(), 1, static_cast<std::size_t>(n), f);
    std::fclose(f);
    return s;
}

static void test_driver_source_attach_safe() {
    std::printf("\n[test] vuln_driver.c attach-safe protocol (shipped source)\n");

    // Resolve driver source next to this test binary or via known relative paths.
    const char* candidates[] = {
        "vuln_driver.c",
        "..\\vuln_driver.c",
        "..\\..\\vuln_driver.c",
        "code\\drivers\\example_vulnerable\\vuln_driver.c",
        "..\\code\\drivers\\example_vulnerable\\vuln_driver.c",
    };
    // Also try path relative to this source file location at build time.
    static const char kSrcDir[] = __FILE__;
    std::string beside = kSrcDir;
    auto slash = beside.find_last_of("/\\");
    if (slash != std::string::npos)
        beside = beside.substr(0, slash + 1) + "vuln_driver.c";

    std::string src = read_file_text(beside.c_str());
    if (src.empty()) {
        for (const char* c : candidates) {
            src = read_file_text(c);
            if (!src.empty()) break;
        }
    }
    EXPECT(!src.empty(), "located shipped vuln_driver.c");
    if (src.empty()) return;

    // VIRT_READ: pool stage + write caller only after detach markers.
    EXPECT(src.find("dRvG") != std::string::npos,
           "ReadProcessMemoryRaw pool tag dRvG present");
    EXPECT(src.find("dWvG") != std::string::npos,
           "WriteProcessMemoryRaw pool tag dWvG present");
    EXPECT(src.find("NEVER touch caller VA while attached") != std::string::npos ||
               src.find("caller_buf is valid again") != std::string::npos,
           "attach-safe comment contract present");

    // Order check for ReadProcessMemoryRaw: ExAllocatePool before attach,
    // ProbeForWrite(caller after KeUnstackDetachProcess.
    auto rpm = src.find("static NTSTATUS ReadProcessMemoryRaw");
    EXPECT(rpm != std::string::npos, "ReadProcessMemoryRaw symbol present");
    if (rpm != std::string::npos) {
        auto end = src.find("static NTSTATUS WriteProcessMemoryRaw", rpm);
        if (end == std::string::npos) end = src.size();
        std::string body = src.substr(rpm, end - rpm);
        auto alloc = body.find("ExAllocatePoolWithTag");
        auto attach = body.find("KeStackAttachProcess");
        auto detach = body.find("KeUnstackDetachProcess");
        auto pwrite = body.find("ProbeForWrite(caller_buf");
        EXPECT(alloc != std::string::npos && attach != std::string::npos &&
                   alloc < attach,
               "VIRT_READ: allocate stage before attach");
        EXPECT(detach != std::string::npos && pwrite != std::string::npos &&
                   detach < pwrite,
               "VIRT_READ: ProbeForWrite(caller) only after detach");
    }

    // WriteProcessMemoryRaw: ProbeForRead(caller) before attach.
    auto wpm = src.find("static NTSTATUS WriteProcessMemoryRaw");
    EXPECT(wpm != std::string::npos, "WriteProcessMemoryRaw symbol present");
    if (wpm != std::string::npos) {
        auto end = src.find("// Helpers: ntoskrnl", wpm);
        if (end == std::string::npos) end = src.find("GdrvGetNtoskrnlInfo", wpm);
        if (end == std::string::npos) end = std::min(src.size(), wpm + 2500);
        std::string body = src.substr(wpm, end - wpm);
        auto pread = body.find("ProbeForRead(caller_buf");
        auto attach = body.find("KeStackAttachProcess");
        EXPECT(pread != std::string::npos && attach != std::string::npos &&
                   pread < attach,
               "VIRT_WRITE: ProbeForRead(caller) before attach");
    }

    // ENTITY_WALK: stage tag, no user ProbeForWrite between attach and detach.
    auto ew = src.find("NTSTATUS GdrvEntityWalk");
    EXPECT(ew != std::string::npos, "GdrvEntityWalk present");
    if (ew != std::string::npos) {
        auto end = src.find("NTSTATUS GdrvProcessScan", ew);
        if (end == std::string::npos) end = src.size();
        std::string body = src.substr(ew, end - ew);
        EXPECT(body.find("'tNeG'") != std::string::npos ||
                   body.find("tNeG") != std::string::npos,
               "ENTITY_WALK stage pool tag tNeG");
        auto attach = body.find("KeStackAttachProcess");
        auto detach = body.find("KeUnstackDetachProcess");
        EXPECT(attach != std::string::npos && detach != std::string::npos &&
                   attach < detach,
               "ENTITY_WALK attach before detach");
        if (attach != std::string::npos && detach != std::string::npos) {
            std::string mid = body.substr(attach, detach - attach);
            // Must not ProbeForWrite user_buffer while attached.
            EXPECT(mid.find("ProbeForWrite((PUCHAR)user_buffer") == std::string::npos &&
                       mid.find("ProbeForWrite(user_buffer") == std::string::npos,
                   "ENTITY_WALK: no user_buffer ProbeForWrite while attached");
            EXPECT(mid.find("stage[") != std::string::npos,
                   "ENTITY_WALK: writes stage[] while attached");
        }
        auto pub = body.find("KeUnstackDetachProcess");
        if (pub != std::string::npos) {
            std::string after = body.substr(pub);
            EXPECT(after.find("ProbeForWrite((PUCHAR)user_buffer") != std::string::npos ||
                       after.find("RtlCopyMemory((PUCHAR)user_buffer") != std::string::npos,
                   "ENTITY_WALK: publish to user_buffer after detach");
        }
    }

    // MODULE_LIST: stage tag + publish after detach.
    auto ml = src.find("NTSTATUS GdrvModuleList");
    EXPECT(ml != std::string::npos, "GdrvModuleList present");
    if (ml != std::string::npos) {
        auto end = src.find("DriverDeviceControl", ml);
        if (end == std::string::npos) end = src.size();
        std::string body = src.substr(ml, end - ml);
        EXPECT(body.find("dLmG") != std::string::npos, "MODULE_LIST stage tag dLmG");
        auto attach = body.find("KeStackAttachProcess");
        auto detach = body.find("KeUnstackDetachProcess");
        EXPECT(attach != std::string::npos && detach != std::string::npos,
               "MODULE_LIST attach/detach present");
        if (attach != std::string::npos && detach != std::string::npos) {
            std::string mid = body.substr(attach, detach - attach);
            EXPECT(mid.find("ProbeForWrite") == std::string::npos,
                   "MODULE_LIST: no ProbeForWrite while attached");
            EXPECT(mid.find("stage[") != std::string::npos,
                   "MODULE_LIST: fills stage[] while attached");
        }
        auto after = body.substr(detach == std::string::npos ? 0 : detach);
        EXPECT(after.find("RtlCopyMemory(user_buffer") != std::string::npos,
               "MODULE_LIST: header write after detach");
    }

    // No hardcoded fake callback counts.
    EXPECT(src.find("count = 12") == std::string::npos, "no fake process cb count 12");
    EXPECT(src.find("count = 15") == std::string::npos, "no fake image cb count 15");
    EXPECT(src.find("simulated") == std::string::npos, "no 'simulated' in driver source");
    EXPECT(src.find("SIMULATED") == std::string::npos, "no 'SIMULATED' in driver source");
}

int main() {
    std::printf("=== gdrv host ABI tests (shipped gdrv_pack / gdrv_abi) ===\n");
    test_ioctl_constants();
    test_struct_sizes();
    test_device_path();
    test_pack_phys();
    test_pack_virt();
    test_pack_entity_and_module();
    test_buffer_macros();
    test_attach_safe_publish();
    test_driver_source_attach_safe();

    std::printf("\n=== Summary: %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
