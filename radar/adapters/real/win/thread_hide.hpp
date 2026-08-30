// thread_hide.hpp — Hide threads from debugger enumeration.
//
// ThreadHideFromDebugger (ThreadInformationClass = 0x11) via
// NtSetInformationThread. After this, debuggers cannot see or break on
// the thread; DbgUi* APIs skip it.
//
// Also provides process-wide hide of the current thread and optional
// hide of all threads in the process via Toolhelp (educational).

#pragma once

#include "real/platform.hpp"

#include <cstdint>

namespace real::win {

/// Hide the calling thread from debuggers. Returns true on NT_SUCCESS.
bool hide_current_thread() noexcept;

/// Hide a specific thread handle (THREAD_SET_INFORMATION access required).
bool hide_thread(void* thread_handle) noexcept;

/// Query whether the current thread is marked hide-from-debugger.
/// Uses NtQueryInformationThread ThreadHideFromDebugger (0x11) when available.
bool is_current_thread_hidden() noexcept;

}  // namespace real::win
