#include "ac_sim/dll_watch.hpp"

#include "real/platform.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#if LR_PLATFORM_WINDOWS
#  include "real/win/api_table.hpp"
#  include "real/win/windows_h.hpp"
#  include "real/win/xorstr.hpp"
#  include <tlhelp32.h>
#else
#  ifndef OBF
#    define OBF(x) x
#  endif
#endif

namespace sim {

#if LR_PLATFORM_WINDOWS
struct DllNotifyData {
    ULONG Flags;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    PVOID DllBase;
    ULONG SizeOfImage;
};

static NTSTATUS NTAPI dll_notify_callback(ULONG reason, PVOID module_info,
                                          PVOID context) {
    auto* watchdog = static_cast<DllWatch*>(context);
    // 1 == LDR_DLL_NOTIFICATION_REASON_LOADED on modern Windows.
    if (!watchdog || (reason != 0 && reason != 1)) return 0;

    auto* info = static_cast<DllNotifyData*>(module_info);
    if (!info || !info->BaseDllName.Buffer) return 0;

    wchar_t wideName[128]{};
    const USHORT nChars =
        info->BaseDllName.Length / sizeof(wchar_t);
    const USHORT copy = nChars < 127 ? nChars : 127;
    wcsncpy(wideName, info->BaseDllName.Buffer, copy);
    wideName[copy] = 0;

    std::string modName;
    modName.reserve(copy);
    for (USHORT i = 0; i < copy; ++i) {
        modName.push_back(static_cast<char>(wideName[i] & 0x7F));
    }

    DllNotification n;
    n.moduleName = modName;
    n.baseAddress = reinterpret_cast<std::uint64_t>(info->DllBase);
    n.isLoad = true;
    if (const char* pat = watchdog->match_pattern(modName.c_str())) {
        n.isSuspicious = true;
        n.matchedPattern = pat;
    }
    watchdog->push_notification(n);
    return 0;
}
#endif

const char* kKnownAcPatterns[] = {
#if LR_PLATFORM_WINDOWS
    OBF("vapor"),         OBF("vac"),           OBF("steamservice"),
    OBF("punkbuster"),    OBF("battleye"),       OBF("beclient"),
    OBF("easyanticheat"), OBF("eac_"),           OBF("easyac"),
    OBF("equ8"),          OBF("faceit"),         OBF("esportal"),
    OBF("medusa"),        OBF("denuvo"),         OBF("nprotect"),
    OBF("xigncode"),      OBF("anticheat"),      OBF("ac-"),
    OBF("crowdstrike"),   OBF("csfalcon"),       OBF("sentinel"),
    OBF("carbonblack"),   OBF("vmware"),         OBF("vbox"),
    OBF("wireshark"),     OBF("cheatengine"),    OBF("cheat eng"),
    OBF("tsearch"),       OBF("reclass"),        OBF("x64dbg"),
    OBF("x32dbg"),        OBF("ollydbg"),        OBF("ida64"),
    OBF("idaq"),          OBF("ghidra"),         OBF("processhacker"),
    OBF("procmon"),       OBF("api monitor")
#else
    "vapor",         "vac",           "steamservice", "punkbuster",
    "battleye",      "beclient",      "easyanticheat","eac_",
    "easyac",        "equ8",          "faceit",       "esportal",
    "medusa",        "denuvo",        "nprotect",     "xigncode",
    "anticheat",     "ac-",           "crowdstrike",  "csfalcon",
    "sentinel",      "carbonblack",   "vmware",       "vbox",
    "wireshark",     "cheatengine",   "cheat eng",    "tsearch",
    "reclass",       "x64dbg",        "x32dbg",       "ollydbg",
    "ida64",         "idaq",          "ghidra",       "processhacker",
    "procmon",       "api monitor"
#endif
};

const std::size_t kKnownAcPatternsCount =
    sizeof(kKnownAcPatterns) / sizeof(kKnownAcPatterns[0]);

void DllWatch::initialize() noexcept {
    m_monitoring = false;
    m_cookie = nullptr;
    m_pending.clear();
    m_seen.clear();
}

void DllWatch::start_monitoring() noexcept {
    if (m_monitoring) return;
#if LR_PLATFORM_WINDOWS
    auto& api = real::win::g_Api();
    if (api.LdrRegisterDllNotification) {
        NTSTATUS status = api.LdrRegisterDllNotification(
            0, dll_notify_callback, this, &m_cookie);
        (void)status;
        m_monitoring = (m_cookie != nullptr);
    } else {
        // Soft-success: pattern matching + poll still work.
        m_monitoring = true;
    }
#else
    m_monitoring = true;
#endif
}

void DllWatch::stop_monitoring() noexcept {
    if (!m_monitoring) return;
#if LR_PLATFORM_WINDOWS
    auto& api = real::win::g_Api();
    if (api.LdrUnregisterDllNotification && m_cookie) {
        api.LdrUnregisterDllNotification(m_cookie);
    }
#endif
    m_monitoring = false;
    m_cookie = nullptr;
}

void DllWatch::push_notification(const std::string& modName) noexcept {
    DllNotification n;
    n.moduleName = modName;
    n.isLoad = true;
    if (const char* pat = match_pattern(modName.c_str())) {
        n.isSuspicious = true;
        n.matchedPattern = pat;
    }
    push_notification(n);
}

void DllWatch::push_notification(const DllNotification& n) noexcept {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pending.push_back(n);
}

std::vector<DllNotification> DllWatch::pending_notifications() noexcept {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    auto result = std::move(m_pending);
    m_pending.clear();
    return result;
}

const char* DllWatch::match_pattern(const char* moduleName) noexcept {
    if (!moduleName) return nullptr;
    std::string name(moduleName);
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    for (std::size_t i = 0; i < kKnownAcPatternsCount; ++i) {
        const char* pattern = kKnownAcPatterns[i];
        if (!pattern) continue;
        if (name.find(pattern) != std::string::npos) {
            return pattern;
        }
    }
    return nullptr;
}

bool DllWatch::is_suspicious_module(const char* moduleName) noexcept {
    return match_pattern(moduleName) != nullptr;
}

int DllWatch::poll_modules() noexcept {
    int hits = 0;
#if LR_PLATFORM_WINDOWS
    // Enumerate loaded modules via Toolhelp — educational blue sensor path.
    HANDLE snap =
        ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                                   ::GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    if (::Module32FirstW(snap, &me)) {
        do {
            char narrow[MAX_MODULE_NAME32 + 1]{};
            for (int i = 0; i < MAX_MODULE_NAME32 && me.szModule[i]; ++i) {
                narrow[i] = static_cast<char>(me.szModule[i] & 0x7F);
            }
            if (!is_suspicious_module(narrow)) continue;

            std::string key(narrow);
            if (std::find(m_seen.begin(), m_seen.end(), key) != m_seen.end()) {
                continue;
            }
            m_seen.push_back(key);

            DllNotification n;
            n.moduleName = key;
            n.baseAddress = reinterpret_cast<std::uint64_t>(me.modBaseAddr);
            n.isLoad = true;
            n.isSuspicious = true;
            if (const char* pat = match_pattern(narrow)) {
                n.matchedPattern = pat;
            }
            push_notification(n);
            ++hits;
        } while (::Module32NextW(snap, &me));
    }
    ::CloseHandle(snap);
#endif
    return hits;
}

void DllWatch::shutdown() noexcept {
    stop_monitoring();
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pending.clear();
    }
    m_seen.clear();
}

} // namespace sim
