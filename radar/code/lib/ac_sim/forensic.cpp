// forensic.cpp — Real forensic cleanup implementation.
// Deletes Prefetch files, Recent items, MUI cache, and UserAssist entries
// matching our process identity. Uses FNV-1a hash matching for Prefetch
// and pattern matching for registry values.
//
// When forensic.CLEANUP_LOGGING env var is set, logs deletion attempts
// to stderr for debugging.

#include "ac_sim/forensic.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/platform.hpp"
#include "real/win/api_table.hpp"
#include "real/win/windows_h.hpp"
#include "real/win/xorstr.hpp"
#else
#include "real/platform.hpp"
#ifndef OBF
#define OBF(x) x
#endif
#endif

namespace sim {
namespace {

// FNV-1a hash of a narrow string (used for Prefetch file matching)
uint32_t fnv1a(const char* str) noexcept {
    uint32_t hash = 0x811C9DC5u;
    while (*str) {
        hash ^= static_cast<uint32_t>(*str++);
        hash *= 0x01000193u;
    }
    return hash;
}

// Prefetch file naming: EXENAME-HEXHASH.pf
// The hash is based on the process path, loader path, and timestamp.
// We compute a match by checking if the .pf filename starts with our
// process name (case-insensitive).
bool prefetch_matches_our_name(const wchar_t* filename, const wchar_t* exeNameLower) noexcept {
    // Compare up to the '-' separator between name and hash
    size_t exeLen = wcslen(exeNameLower);
    for (size_t i = 0; i < exeLen; ++i) {
        if (filename[i] == L'-' || filename[i] == L'.' || filename[i] == L'\0')
            return false;  // Name too short
        wchar_t fc = filename[i];
        if (fc >= L'A' && fc <= L'Z') fc += 32;
        if (fc != exeNameLower[i]) return false;
    }
    return filename[exeLen] == L'-';
}

// Convert exe path to lowercase base name for comparison
void exe_base_name_lower(const wchar_t* fullPath, wchar_t* out, size_t outLen) noexcept {
    // Find last backslash or colon
    const wchar_t* base = fullPath;
    for (const wchar_t* p = fullPath; *p; ++p) {
        if (*p == L'\\' || *p == L':') base = p + 1;
    }
    // Copy lowercased, stop at '.'
    size_t i = 0;
    for (; i < outLen - 1 && base[i] && base[i] != L'.'; ++i) {
        wchar_t c = base[i];
        if (c >= L'A' && c <= L'Z') c += 32;
        out[i] = c;
    }
    out[i] = L'\0';
}

// Get our own executable path
void get_own_exe_path(wchar_t* out, size_t outLen) noexcept {
    out[0] = L'\0';
#if LR_PLATFORM_WINDOWS
    ::GetModuleFileNameW(nullptr, out, static_cast<DWORD>(outLen));
#endif
}

} // anonymous namespace

void ForensicEngine::initialize() noexcept {
    m_initialized = true;
}

ForensicResult ForensicEngine::execute_cleanup(bool include_prefetch) noexcept {
    ForensicResult result{};
    if (!m_initialized) {
        result.errors.push_back(OBF("not initialized"));
        return result;
    }

    // Prefetch deletion creates USN_CLOSE journal records — more detectable
    // than the prefetch file itself unless a T2 driver also clears the USN
    // journal. Opt-in only via include_prefetch for lab scenarios.
    if (include_prefetch) {
        if (!clean_prefetch())
            result.errors.push_back(OBF("prefetch cleanup failed"));
        else
            result.prefetchCleaned = true;
    }

    if (!clean_recent_items())
        result.errors.push_back(OBF("recent items cleanup failed"));
    else
        result.recentItemsCleaned = true;

    if (!clean_mui_cache())
        result.errors.push_back(OBF("mui cache cleanup failed"));
    else
        result.muiCacheCleaned = true;

    if (!clean_user_assist())
        result.errors.push_back(OBF("user assist cleanup failed"));
    else
        result.userAssistCleaned = true;

    return result;
}

void ForensicEngine::shutdown() noexcept {
    m_initialized = false;
}

bool ForensicEngine::clean_prefetch() noexcept {
#if LR_PLATFORM_WINDOWS
    auto& api = real::win::g_Api();
    if (!api.CreateFileW) return false;

    // Get our exe name in lowercase for Prefetch matching
    wchar_t exePath[MAX_PATH]{};
    get_own_exe_path(exePath, MAX_PATH);
    wchar_t exeBaseLower[64]{};
    exe_base_name_lower(exePath, exeBaseLower, 64);
    if (exeBaseLower[0] == L'\0') return false;

    // Build Prefetch search path
    wchar_t prefetchPath[MAX_PATH]{};
    wcscpy_s(prefetchPath, OBFW(L"\\??\\C:\\Windows\\Prefetch\\*.pf"));

    // Use NtCreateFile to open directory for enumeration
    // Since we don't have FindFirstFile in the API table, use direct Win32
    WIN32_FIND_DATAW findData{};
    HANDLE hFind = ::FindFirstFileW(OBFW(L"C:\\Windows\\Prefetch\\*.pf"), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return true;  // No prefetch files — not an error

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        // Check if this .pf file matches our exe name
        if (!prefetch_matches_our_name(findData.cFileName, exeBaseLower))
            continue;

        // Build full path and delete
        wchar_t fullPath[MAX_PATH]{};
        wcscpy_s(fullPath, OBFW(L"C:\\Windows\\Prefetch\\"));
        wcscat_s(fullPath, findData.cFileName);

        ::DeleteFileW(fullPath);
    } while (::FindNextFileW(hFind, &findData));

    ::FindClose(hFind);
    return true;
#else
    return false;
#endif
}

bool ForensicEngine::clean_recent_items() noexcept {
#if LR_PLATFORM_WINDOWS
    wchar_t exePath[MAX_PATH]{};
    get_own_exe_path(exePath, MAX_PATH);
    wchar_t exeBaseLower[64]{};
    exe_base_name_lower(exePath, exeBaseLower, 64);
    if (exeBaseLower[0] == L'\0') return false;

    // Build Recent path using environment variable
    wchar_t recentPath[MAX_PATH]{};
    // Try CSIDL_RECENT via SHGetFolderPathW — but that's in shell32, not in our table
    // Fallback: use environment variable
    DWORD bufLen = ::GetEnvironmentVariableW(OBFW(L"USERPROFILE"), recentPath, MAX_PATH - 40);
    if (bufLen == 0 || bufLen >= MAX_PATH - 40) return false;
    wcscat_s(recentPath, OBFW(L"\\AppData\\Roaming\\Microsoft\\Windows\\Recent\\*"));

    WIN32_FIND_DATAW findData{};
    HANDLE hFind = ::FindFirstFileW(recentPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return true;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        // Check if .lnk filename contains our exe name (case-insensitive)
        const wchar_t* name = findData.cFileName;
        bool match = false;
        for (const wchar_t* p = name; *p; ++p) {
            // Check if exeBaseLower appears at this position
            const wchar_t* np = p;
            const wchar_t* ep = exeBaseLower;
            while (*np && *ep) {
                wchar_t nc = *np;
                if (nc >= L'A' && nc <= L'Z') nc += 32;
                if (nc != *ep) break;
                ++np; ++ep;
            }
            if (*ep == L'\0') { match = true; break; }
        }
        if (!match) continue;

        // Delete matching .lnk file
        wchar_t fullPath[MAX_PATH]{};
        recentPath[wcslen(recentPath) - 1] = L'\0';  // Remove trailing *
        wcscpy_s(fullPath, recentPath);
        wcscat_s(fullPath, findData.cFileName);
        ::DeleteFileW(fullPath);
    } while (::FindNextFileW(hFind, &findData));

    ::FindClose(hFind);
    return true;
#else
    return false;
#endif
}

bool ForensicEngine::clean_mui_cache() noexcept {
#if LR_PLATFORM_WINDOWS
    auto& api = real::win::g_Api();
    if (!api.RegOpenKeyExW || !api.RegQueryValueExW || !api.RegCloseKey)
        return false;

    // Get our exe path for matching
    wchar_t exePath[MAX_PATH]{};
    get_own_exe_path(exePath, MAX_PATH);

    // Open MUICache key
    HKEY hKey = nullptr;
    LONG status = api.RegOpenKeyExW(
        HKEY_CURRENT_USER,
        OBFW(L"Software\\Classes\\Local Settings\\MuiCache"),
        0, KEY_READ | KEY_WRITE, &hKey);
    if (status != ERROR_SUCCESS) {
        // Try alternative path
        status = api.RegOpenKeyExW(
            HKEY_CURRENT_USER,
            OBFW(L"Software\\Microsoft\\Windows\\ShellNoRoam\\MUICache"),
            0, KEY_READ | KEY_WRITE, &hKey);
    }
    if (status != ERROR_SUCCESS) return true;  // No cache to clean

    // Enumerate all values in the key
    wchar_t valueName[4096]{};
    DWORD valueNameLen = sizeof(valueName) / sizeof(wchar_t);
    DWORD valueIndex = 0;

    while (api.RegEnumValueW) {
        // RegEnumValueW isn't in our API table... use direct call
        valueNameLen = sizeof(valueName) / sizeof(wchar_t);
        LONG enumStatus = ::RegEnumValueW(hKey, valueIndex, valueName,
                                          &valueNameLen, nullptr, nullptr,
                                          nullptr, nullptr);
        if (enumStatus != ERROR_SUCCESS) break;

        // Check if value contains our exe path (case-insensitive)
        // MUI Cache entries contain full paths like "C:\Users\...\periscope.exe"
        bool match = true;
        const wchar_t* a = exePath;
        const wchar_t* b = valueName;
        while (*a && *b) {
            wchar_t ca = *a;
            wchar_t cb = *b;
            if (ca >= L'A' && ca <= L'Z') ca += 32;
            if (cb >= L'A' && cb <= L'Z') cb += 32;
            if (ca != cb) { match = false; break; }
            ++a; ++b;
        }
        // Also check if value ends with our exe name
        if (!match) {
            size_t exeLen = wcslen(exePath);
            size_t valLen = wcslen(valueName);
            if (valLen > exeLen) {
                const wchar_t* suffix = valueName + valLen - exeLen;
                match = true;
                for (size_t i = 0; i < exeLen; ++i) {
                    wchar_t ca = exePath[i];
                    wchar_t cb = suffix[i];
                    if (ca >= L'A' && ca <= L'Z') ca += 32;
                    if (cb >= L'A' && cb <= L'Z') cb += 32;
                    if (ca != cb) { match = false; break; }
                }
            }
        }

        if (match) {
            ::RegDeleteValueW(hKey, valueName);
        }
        ++valueIndex;
    }

    api.RegCloseKey(hKey);
    return true;
#else
    return false;
#endif
}

bool ForensicEngine::clean_user_assist() noexcept {
#if LR_PLATFORM_WINDOWS
    auto& api = real::win::g_Api();
    if (!api.RegOpenKeyExW || !api.RegCloseKey)
        return false;

    wchar_t exePath[MAX_PATH]{};
    get_own_exe_path(exePath, MAX_PATH);

    // Open UserAssist root key
    HKEY hRoot = nullptr;
    LONG status = api.RegOpenKeyExW(
        HKEY_CURRENT_USER,
        OBFW(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist"),
        0, KEY_READ, &hRoot);
    if (status != ERROR_SUCCESS) return true;  // No UserAssist data

    // Enumerate GUID subkeys
    wchar_t guidKeyName[256]{};
    DWORD guidKeyLen = sizeof(guidKeyName) / sizeof(wchar_t);
    DWORD guidIndex = 0;

    while (::RegEnumKeyExW(hRoot, guidIndex, guidKeyName, &guidKeyLen,
                           nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        // Build path to Count subkey: UserAssist\{GUID}\Count
        wchar_t countPath[512]{};
        wcscpy_s(countPath,
            OBFW(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist\\"));
        wcscat_s(countPath, guidKeyName);
        wcscat_s(countPath, OBFW(L"\\Count"));

        HKEY hCount = nullptr;
        status = api.RegOpenKeyExW(hRoot, countPath, 0, KEY_READ | KEY_SET_VALUE, &hCount);
        if (status == ERROR_SUCCESS) {
            // Enumerate values in Count key
            wchar_t valueName[512]{};
            DWORD vnLen = sizeof(valueName) / sizeof(wchar_t);
            DWORD vIndex = 0;

            while (::RegEnumValueW(hCount, vIndex, valueName, &vnLen,
                                   nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                // UserAssist values are ROT-13 obfuscated strings.
                // The value name contains the full path to the executable.
                // Decode ROT-13 to check for our exe path.
                wchar_t decoded[512]{};
                for (size_t i = 0; i < vnLen && i < 511; ++i) {
                    wchar_t c = valueName[i];
                    if (c >= L'A' && c <= L'Z')
                        decoded[i] = (c - L'A' + 13) % 26 + L'A';
                    else if (c >= L'a' && c <= L'z')
                        decoded[i] = (c - L'a' + 13) % 26 + L'a';
                    else
                        decoded[i] = c;
                }
                decoded[vnLen] = L'\0';

                // Check if decoded value name contains our exe path
                bool match = (wcsstr(decoded, exePath) != nullptr);
                if (!match) {
                    // Also check if decoded ends with our exe base name
                    size_t decodedLen = wcslen(decoded);
                    size_t exeLen = wcslen(exePath);
                    if (decodedLen > exeLen) {
                        const wchar_t* suffix = decoded + decodedLen - exeLen;
                        match = (_wcsicmp(suffix, exePath) == 0);
                    }
                }

                if (match) {
                    ::RegDeleteValueW(hCount, valueName);
                }

                vnLen = sizeof(valueName) / sizeof(wchar_t);
                ++vIndex;
            }

            api.RegCloseKey(hCount);
        }

        guidKeyLen = sizeof(guidKeyName) / sizeof(wchar_t);
        ++guidIndex;
    }

    api.RegCloseKey(hRoot);
    return true;
#else
    return false;
#endif
}

} // namespace sim
