// windows_h.hpp — Single point for <windows.h> inclusion.
// All real/win/ sources include this instead of <windows.h> directly.
// Defines necessary WIN32_LEAN_AND_MEAN and version macros once.

#pragma once

#if LR_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0A00  // Windows 10+
#endif

#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <processthreadsapi.h>
#include <memoryapi.h>
#include <handleapi.h>
#include <errhandlingapi.h>
#include <synchapi.h>
#include <fileapi.h>
#include <ioapiset.h>

// psapi.h remaps several identifiers to K32* exports via macros, e.g.
//   #define EnumProcessModules K32EnumProcessModules
// That breaks ApiTable member names (this->EnumProcessModules becomes
// this->K32EnumProcessModules). Strip the macros; call sites that need
// the import can use the K32* names or ApiTable function pointers.
#ifdef EmptyWorkingSet
#  undef EmptyWorkingSet
#endif
#ifdef EnumProcessModules
#  undef EnumProcessModules
#endif
#ifdef EnumProcessModulesEx
#  undef EnumProcessModulesEx
#endif
#ifdef GetModuleBaseNameA
#  undef GetModuleBaseNameA
#endif
#ifdef GetModuleBaseNameW
#  undef GetModuleBaseNameW
#endif
#ifdef GetModuleFileNameExA
#  undef GetModuleFileNameExA
#endif
#ifdef GetModuleFileNameExW
#  undef GetModuleFileNameExW
#endif
#ifdef GetModuleInformation
#  undef GetModuleInformation
#endif
#ifdef GetProcessImageFileNameA
#  undef GetProcessImageFileNameA
#endif
#ifdef GetProcessImageFileNameW
#  undef GetProcessImageFileNameW
#endif

// Shared NT type definitions (CLIENT_ID, OBJECT_ATTRIBUTES, etc.)
// Included AFTER Windows headers so fallbacks are not emitted.
#include "real/win/nt_types.hpp"

// Prefer the SDK enum value when available (winternl: SystemProcessInformation = 5).
#ifndef SystemProcessInformation
#  define SystemProcessInformation 5
#endif

// NTSTATUS success code (not always exposed via winternl under LEAN_AND_MEAN).
#ifndef STATUS_SUCCESS
#  define STATUS_SUCCESS ((LONG)0x00000000L)
#endif

#else
// Stub for non-Windows builds
#include <cstdint>
#include <cstddef>
#endif
