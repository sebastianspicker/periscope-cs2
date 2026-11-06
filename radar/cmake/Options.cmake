# Radar product options: real backends, compiler hardening. Lab/simulation
# options and the simulation seed generation live in
# anti-cheat/cmake/Options.cmake.
#
# Include order matters: include anti-cheat's LrHelpers.cmake FIRST (it creates
# lr_build_options), then anti-cheat's Options.cmake, then this file. Nothing
# here may re-define what LrHelpers.cmake owns (lr_build_options, lr_library,
# lr_exe, platform compile definitions).

option(LR_ENABLE_VEH "Enable VEH anti-debug handler (kernel-visible)" OFF)

option(LR_ENABLE_REAL_RPM "Build real Windows ReadProcessMemory backend" OFF)
option(LR_ENABLE_REAL_SYSCALL "Build real direct syscall backend" OFF)
option(LR_ENABLE_REAL_KERNEL "Build real kernel/BYOVD backend" OFF)
option(LR_ENABLE_REAL_VMX "Build real Intel VT-x hypervisor backend" OFF)
option(LR_ENABLE_REAL_DMA "Build real DMA/PCIe backend" OFF)
option(LR_ENABLE_REAL_SMM "Build real SMM/firmware backend" OFF)
option(LR_ENABLE_REAL_GPU "Build real GPU rendering/compute backends" OFF)
option(LR_ENABLE_REAL_NET "Build real networking/C2 backends" OFF)
option(LR_ENABLE_REAL_UEFI "Build UEFI firmware backend" OFF)
option(LR_ENABLE_REAL_LINUX "Build Linux real backend and host stubs" OFF)
option(LR_ENABLE_REAL_ALL "Enable ALL real backends (use with care)" OFF)
option(LR_ENABLE_SHELLCODE_INJECTION "Build the isolated Windows x64 shellcode research adapter" OFF)

if(LR_ENABLE_REAL_ALL)
  foreach(opt RPM SYSCALL KERNEL VMX DMA SMM GPU NET UEFI LINUX)
    set(LR_ENABLE_REAL_${opt} ON)
  endforeach()
endif()

foreach(opt LR_ENABLE_VEH)
  if(${opt})
    target_compile_definitions(lr_build_options INTERFACE ${opt}=1)
  endif()
endforeach()

set(LR_REAL_ENABLED OFF)
foreach(opt RPM SYSCALL KERNEL VMX DMA SMM GPU NET UEFI LINUX)
  if(LR_ENABLE_REAL_${opt})
    set(LR_REAL_ENABLED ON)
    target_compile_definitions(lr_build_options INTERFACE LR_HAS_REAL_${opt}=1)
  endif()
endforeach()
if(APPLE AND LR_REAL_ENABLED)
  message(FATAL_ERROR "Real backend features are unsupported on Apple. Leave LR_ENABLE_REAL_* options OFF.")
endif()
if(LR_ENABLE_SHELLCODE_INJECTION)
  if(NOT LR_ENABLE_REAL_SYSCALL)
    message(FATAL_ERROR "LR_ENABLE_SHELLCODE_INJECTION requires LR_ENABLE_REAL_SYSCALL=ON.")
  endif()
  if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "LR_ENABLE_SHELLCODE_INJECTION requires a 64-bit Windows lab host.")
  endif()
  target_compile_definitions(lr_build_options INTERFACE LR_HAS_REAL_SHELLCODE=1)
else()
  target_compile_definitions(lr_build_options INTERFACE LR_HAS_REAL_SHELLCODE=0)
endif()

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /DNDEBUG /GS /GR-")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DYNAMICBASE /guard:cf /HIGHENTROPYVA /NXCOMPAT /PDBSTRIPPED /PDBALTPATH:NUL /CETCOMPAT /MANIFEST:NO")
else()
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -fvisibility=hidden -ffunction-sections -fdata-sections")
  if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,-dead_strip")
  else()
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
  endif()
endif()
