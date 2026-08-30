#pragma once

// Simulation-owned platform facts.  Keeping these declarations here prevents
// the default lab graph from importing a real adapter merely to learn its OS.
#ifndef LR_PLATFORM_WINDOWS
#  if defined(_WIN32) || defined(_WIN64)
#    define LR_PLATFORM_WINDOWS 1
#  else
#    define LR_PLATFORM_WINDOWS 0
#  endif
#endif
#ifndef LR_PLATFORM_LINUX
#  if defined(__linux__)
#    define LR_PLATFORM_LINUX 1
#  else
#    define LR_PLATFORM_LINUX 0
#  endif
#endif
#ifndef LR_ARCH_X64
#  if defined(_M_AMD64) || defined(__x86_64__)
#    define LR_ARCH_X64 1
#  else
#    define LR_ARCH_X64 0
#  endif
#endif
#ifndef LR_HAS_REAL_PLATFORM
#  define LR_HAS_REAL_PLATFORM 0
#endif
