#pragma once
// Shared NT type definitions for the project.
// Prefer SDK types from windows_h.hpp / winternl.h when available.
// Only define fallbacks when Windows headers have NOT been included.

#include <cstdint>

#if defined(LR_PLATFORM_WINDOWS) && LR_PLATFORM_WINDOWS

// winternl.h provides CLIENT_ID / OBJECT_ATTRIBUTES / POBJECT_ATTRIBUTES
// but not always PCLIENT_ID. Only emit full fallbacks when SDK types absent.
#if !defined(_WINTERNL_)

#ifndef _PCLIENT_ID_DEFINED
#define _PCLIENT_ID_DEFINED
typedef struct _CLIENT_ID {
    void* UniqueProcess;
    void* UniqueThread;
} CLIENT_ID;
typedef CLIENT_ID* PCLIENT_ID;
#endif

#ifndef _OBJECT_ATTRIBUTES_DEFINED
#define _OBJECT_ATTRIBUTES_DEFINED
typedef struct _OBJECT_ATTRIBUTES {
    uint32_t Length;
    void* RootDirectory;
    void* ObjectName;
    uint32_t Attributes;
    void* SecurityDescriptor;
    void* SecurityQualityOfService;
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES* POBJECT_ATTRIBUTES;
#endif

#else
// SDK present: provide PCLIENT_ID if the kit omitted it.
#ifndef _PCLIENT_ID_DEFINED
#define _PCLIENT_ID_DEFINED
typedef CLIENT_ID* PCLIENT_ID;
#endif
#endif // !_WINTERNL_

#endif // LR_PLATFORM_WINDOWS
