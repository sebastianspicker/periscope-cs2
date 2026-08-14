// platform.hpp — Single platform-detection header.
// All real-backend code includes this and uses LR_PLATFORM_* macros.
// No platform-specific #ifdefs should appear outside real/ and src/real/.

#pragma once

// --- Platform detection ---
#if defined(_WIN32) || defined(_WIN64)
#  define LR_PLATFORM_WINDOWS 1
#  define LR_PLATFORM_LINUX   0
#elif defined(__linux__)
#  define LR_PLATFORM_WINDOWS 0
#  define LR_PLATFORM_LINUX   1
#else
#  define LR_PLATFORM_WINDOWS 0
#  define LR_PLATFORM_LINUX   0
#endif

// --- Architecture detection ---
#if defined(_M_AMD64) || defined(__x86_64__)
#  define LR_ARCH_X64     1
#  define LR_ARCH_X86     0
#  define LR_ARCH_ARM64   0
#elif defined(_M_IX86) || defined(__i386__)
#  define LR_ARCH_X64     0
#  define LR_ARCH_X86     1
#  define LR_ARCH_ARM64   0
#elif defined(_M_ARM64) || defined(__aarch64__)
#  define LR_ARCH_X64     0
#  define LR_ARCH_X86     0
#  define LR_ARCH_ARM64   1
#else
#  define LR_ARCH_X64     0
#  define LR_ARCH_X86     0
#  define LR_ARCH_ARM64   0
#endif

// --- Compiler detection ---
#if defined(_MSC_VER)
#  define LR_COMPILER_MSVC  1
#  define LR_COMPILER_GCC   0
#  define LR_COMPILER_CLANG 0
#elif defined(__clang__)
#  define LR_COMPILER_MSVC  0
#  define LR_COMPILER_GCC   0
#  define LR_COMPILER_CLANG 1
#elif defined(__GNUC__)
#  define LR_COMPILER_MSVC  0
#  define LR_COMPILER_GCC   1
#  define LR_COMPILER_CLANG 0
#else
#  define LR_COMPILER_MSVC  0
#  define LR_COMPILER_GCC   0
#  define LR_COMPILER_CLANG 0
#endif

// --- Feature macros (set by CMake) ---
// These are defined as compile definitions when the corresponding
// LR_ENABLE_REAL_* CMake option is ON.
// #define LR_HAS_REAL_RPM
// #define LR_HAS_REAL_SYSCALL
// #define LR_HAS_REAL_KERNEL
// #define LR_HAS_REAL_VMX
// #define LR_HAS_REAL_DMA
// #define LR_HAS_REAL_SMM
// #define LR_HAS_REAL_GPU
// #define LR_HAS_REAL_NET
